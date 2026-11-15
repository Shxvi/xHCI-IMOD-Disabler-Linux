// SPDX-License-Identifier: GPL-2.0-only
/*
 * xHCI Interrupt Moderation (IMOD) Controller and Disabler Kernel Module
 *
 * Allows viewing and modifying the xHCI Interrupt Moderation (IMOD) register
 * on Linux USB 3.0/3.1/3.2 host controllers to achieve true 0 µs interrupt latency.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xHCI-IMOD-Disabler Linux Port");
MODULE_DESCRIPTION("xHCI Interrupt Moderation (IMOD) Disabler");
MODULE_VERSION("1.2");

// Module parameters
static unsigned int imod_interval = 0; // 0 = disabled (0 us)
module_param(imod_interval, uint, 0644);
MODULE_PARM_DESC(imod_interval, "Desired IMOD interval in 250ns increments (0 = moderation disabled)");

static bool restore_on_unload = true;
module_param(restore_on_unload, bool, 0644);
MODULE_PARM_DESC(restore_on_unload, "Restore original IMOD values on module unload (default: true)");

// xHCI Register offsets
#define XHCI_HCSPARAMS1         0x04
#define XHCI_RTSOFF             0x18
#define XHCI_INTERRUPTER_OFFSET 0x20
#define XHCI_INTERRUPTER_SIZE   0x20
#define XHCI_IMAN_OFFSET        0x00
#define XHCI_IMOD_OFFSET        0x04
#define XHCI_IMOD_INTERVAL_MASK 0x0000FFFF
#define XHCI_MAX_INTERRUPTERS   1024

struct xhci_ctrl_node {
    struct pci_dev *pdev;
    char bdf[32];
    void __iomem *bar0;
    resource_size_t bar0_len;
    u32 rtsoff;
    u32 num_interrupters;
    u32 *original_imod;
    struct kobject kobj;
    struct list_head list;
};

static LIST_HEAD(ctrl_list);
static DEFINE_MUTEX(ctrl_mutex);
static struct kobject *xhci_imod_root_kobj;

static u32 read_imod_reg(struct xhci_ctrl_node *node, u32 interrupter)
{
    u32 offset = node->rtsoff + XHCI_INTERRUPTER_OFFSET + (interrupter * XHCI_INTERRUPTER_SIZE) + XHCI_IMOD_OFFSET;
    if (offset + sizeof(u32) > node->bar0_len)
        return 0;
    return readl(node->bar0 + offset) & XHCI_IMOD_INTERVAL_MASK;
}

static void write_imod_reg(struct xhci_ctrl_node *node, u32 interrupter, u32 val)
{
    u32 offset = node->rtsoff + XHCI_INTERRUPTER_OFFSET + (interrupter * XHCI_INTERRUPTER_SIZE) + XHCI_IMOD_OFFSET;
    if (offset + sizeof(u32) > node->bar0_len)
        return;
    writel(val & XHCI_IMOD_INTERVAL_MASK, node->bar0 + offset);
}

// sysfs show/store for "imod"
static ssize_t imod_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct xhci_ctrl_node *node = container_of(kobj, struct xhci_ctrl_node, kobj);
    u32 cur_val = read_imod_reg(node, 0);
    return scnprintf(buf, PAGE_SIZE, "0x%04X (%u units, %.2f us)\n",
                     cur_val, cur_val, (float)cur_val * 0.25f);
}

static ssize_t imod_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    struct xhci_ctrl_node *node = container_of(kobj, struct xhci_ctrl_node, kobj);
    unsigned int new_val = 0;
    u32 i;
    int ret = kstrtouint(buf, 0, &new_val);
    if (ret)
        return ret;

    mutex_lock(&ctrl_mutex);
    for (i = 0; i < node->num_interrupters; ++i) {
        write_imod_reg(node, i, new_val);
    }
    mutex_unlock(&ctrl_mutex);

    pr_info("xhci_imod: [%s] updated IMOD to 0x%04X (%u units)\n", node->bdf, new_val, new_val);
    return count;
}
static struct kobj_attribute imod_attr = __ATTR(imod, 0644, imod_show, imod_store);

// sysfs show for "info"
static ssize_t info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct xhci_ctrl_node *node = container_of(kobj, struct xhci_ctrl_node, kobj);
    return scnprintf(buf, PAGE_SIZE,
                     "BDF: %s\n"
                     "Vendor: 0x%04x\n"
                     "Device: 0x%04x\n"
                     "Interrupters: %u\n"
                     "BAR0 Base: 0x%llx\n"
                     "BAR0 Size: 0x%llx\n"
                     "RTSOFF: 0x%08x\n",
                     node->bdf,
                     node->pdev->vendor,
                     node->pdev->device,
                     node->num_interrupters,
                     (unsigned long long)pci_resource_start(node->pdev, 0),
                     (unsigned long long)node->bar0_len,
                     node->rtsoff);
}
static struct kobj_attribute info_attr = __ATTR_RO(info);

static struct attribute *ctrl_attrs[] = {
    &imod_attr.attr,
    &info_attr.attr,
    NULL,
};

static struct attribute_group ctrl_attr_group = {
    .attrs = ctrl_attrs,
};

static int init_controller(struct pci_dev *pdev)
{
    struct xhci_ctrl_node *node;
    resource_size_t start, len;
    u32 header, hcsparams1, rtsoff, num_intrs, i;
    int ret;

    start = pci_resource_start(pdev, 0);
    len = pci_resource_len(pdev, 0);
    if (!start || !len)
        return -ENODEV;

    node = kzalloc(sizeof(*node), GFP_KERNEL);
    if (!node)
        return -ENOMEM;

    node->pdev = pdev;
    snprintf(node->bdf, sizeof(node->bdf), "%s", pci_name(pdev));
    node->bar0_len = len;

    node->bar0 = ioremap(start, len);
    if (!node->bar0) {
        pr_err("xhci_imod: [%s] failed to ioremap BAR0 (0x%llx)\n", node->bdf, (unsigned long long)start);
        kfree(node);
        return -EIO;
    }

    header = readl(node->bar0);
    hcsparams1 = readl(node->bar0 + XHCI_HCSPARAMS1);
    rtsoff = readl(node->bar0 + XHCI_RTSOFF) & ~0x1Fu;

    num_intrs = (hcsparams1 >> 8) & 0x7FF;
    if (num_intrs == 0 || num_intrs > XHCI_MAX_INTERRUPTERS)
        num_intrs = 1;

    node->rtsoff = rtsoff;
    node->num_interrupters = num_intrs;

    node->original_imod = kcalloc(num_intrs, sizeof(u32), GFP_KERNEL);
    if (!node->original_imod) {
        iounmap(node->bar0);
        kfree(node);
        return -ENOMEM;
    }

    // Read and save original IMOD values, then apply imod_interval
    for (i = 0; i < num_intrs; ++i) {
        node->original_imod[i] = read_imod_reg(node, i);
        write_imod_reg(node, i, imod_interval);
    }

    pr_info("xhci_imod: [%s] mapped %u interrupter(s), IMOD set: 0x%04X (orig: 0x%04X)\n",
            node->bdf, num_intrs, imod_interval, node->original_imod[0]);

    // Create sysfs directory under /sys/kernel/xhci_imod/<bdf>
    ret = kobject_init_and_add(&node->kobj, NULL, xhci_imod_root_kobj, "%s", node->bdf);
    if (ret) {
        kobject_put(&node->kobj);
        kfree(node->original_imod);
        iounmap(node->bar0);
        kfree(node);
        return ret;
    }

    ret = sysfs_create_group(&node->kobj, &ctrl_attr_group);
    if (ret) {
        kobject_put(&node->kobj);
        kfree(node->original_imod);
        iounmap(node->bar0);
        kfree(node);
        return ret;
    }

    list_add_tail(&node->list, &ctrl_list);
    return 0;
}

static int __init xhci_imod_init(void)
{
    struct pci_dev *pdev = NULL;
    int count = 0;

    pr_info("xhci_imod: initializing module, target interval: 0x%04X\n", imod_interval);

    xhci_imod_root_kobj = kobject_create_and_add("xhci_imod", kernel_kobj);
    if (!xhci_imod_root_kobj)
        return -ENOMEM;

    mutex_lock(&ctrl_mutex);
    while ((pdev = pci_get_class(PCI_CLASS_SERIAL_USB_XHCI, pdev)) != NULL) {
        if (init_controller(pdev) == 0)
            count++;
    }
    mutex_unlock(&ctrl_mutex);

    pr_info("xhci_imod: configured %d xHCI controller(s)\n", count);
    return 0;
}

static void __exit xhci_imod_exit(void)
{
    struct xhci_ctrl_node *node, *tmp;
    u32 i;

    mutex_lock(&ctrl_mutex);
    list_for_each_entry_safe(node, tmp, &ctrl_list, list) {
        if (restore_on_unload && node->original_imod) {
            for (i = 0; i < node->num_interrupters; ++i) {
                write_imod_reg(node, i, node->original_imod[i]);
            }
            pr_info("xhci_imod: [%s] restored original IMOD 0x%04X\n",
                    node->bdf, node->original_imod[0]);
        }

        sysfs_remove_group(&node->kobj, &ctrl_attr_group);
        kobject_put(&node->kobj);

        if (node->original_imod)
            kfree(node->original_imod);
        if (node->bar0)
            iounmap(node->bar0);

        list_del(&node->list);
        kfree(node);
    }
    mutex_unlock(&ctrl_mutex);

    if (xhci_imod_root_kobj)
        kobject_put(xhci_imod_root_kobj);

    pr_info("xhci_imod: unloaded\n");
}

module_init(xhci_imod_init);
module_exit(xhci_imod_exit);
