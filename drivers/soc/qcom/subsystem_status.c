// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <soc/qcom/subsystem_sleep_stats.h>

static int subsys_syscore_suspend(void)
{
	return 0;
}

struct syscore_ops subsys_sleep_syscore_ops = {
	.suspend         = subsys_syscore_suspend,
};

static int qcom_subsys_status_probe(struct platform_device *pdev)
{
	register_syscore_ops(&subsys_sleep_syscore_ops);
	subsystem_sleep_debug_enable(true);

	return 0;
}

static int qcom_subsys_status_remove(struct platform_device *pdev)
{
	return 0;
}

static int qcom_subsys_suspend_noirq(struct device *dev)
{
	bool subsys_sleep_status = current_subsystem_sleep();

	pr_info("%s: All subsystems slept: %d\n", __func__, subsys_sleep_status);
	return !subsys_sleep_status;
}

static struct platform_device qcom_subsys_status_device = {
	.name = "qcom-subsystem-status",
	.id = -1,
};

static const struct dev_pm_ops qcom_subsys_dev_pm_ops = {
	.suspend_noirq = qcom_subsys_suspend_noirq,
};

static struct platform_driver qcom_subsys_status_driver = {
	.probe = qcom_subsys_status_probe,
	.remove = qcom_subsys_status_remove,
	.driver  = {
		.name = "qcom-subsystem-status",
		.pm = &qcom_subsys_dev_pm_ops,
	},
};

static int __init qcom_subsys_status_driver_init(void)
{
	int ret = 0;

	ret = platform_device_register(&qcom_subsys_status_device);
	if (ret)
		return -ENODEV;

	ret = platform_driver_register(&qcom_subsys_status_driver);
	if (ret) {
		pr_err("%s: Failed to register qcom_subsys_status_driver\n",
								__func__);
	}

	return ret;
}

static void __exit qcom_subsys_status_driver_exit(void)
{
	platform_driver_unregister(&qcom_subsys_status_driver);

	platform_device_unregister(&qcom_subsys_status_device);
}

module_init(qcom_subsys_status_driver_init);
module_exit(qcom_subsys_status_driver_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Subsystem deepsleep status driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qcom-subsystem-status");
MODULE_SOFTDEP("pre: subsystem_sleep_stats");
