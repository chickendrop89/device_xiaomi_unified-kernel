// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */
/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/of_irq.h>
#include <linux/delay.h>

#include "tc956xmac.h"

struct tc956x_qcom_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;
	struct regulator *phy_supply;
	u32 phy_rst_gpio;
	u32 phy_rst_delay_us;
	int wol_irq;
	bool is_built_in_wol;
	bool phy_reverse_rst;
};

static inline struct tc956x_qcom_priv *to_priv(struct tc956xmac_priv *priv)
{
	return (struct tc956x_qcom_priv *)priv->plat_priv;
}

static int tc956x_assert_phy_reset(struct tc956xmac_priv *priv)
{
	return tc956x_GPIO_OutputConfigPin(priv, to_priv(priv)->phy_rst_gpio,
					   0);
}

static int tc956x_deassert_phy_reset(struct tc956xmac_priv *priv)
{
	return tc956x_GPIO_OutputConfigPin(priv, to_priv(priv)->phy_rst_gpio,
					   1);
}

static int tc956x_phy_power_on(struct tc956xmac_priv *priv)
{
	int ret = 0;
	struct tc956x_qcom_priv *qpriv = to_priv(priv);

	ret = regulator_enable(to_priv(priv)->phy_supply);
	if (ret) {
		dev_err(priv->device,
			"Failed to enable PHY supply with error %d\n", ret);
		return ret;
	}
	if (qpriv->phy_reverse_rst) {
		dev_info(priv->device, "QPS615 PHY reverse reset\n");
		ret = tc956x_assert_phy_reset(priv);
	} else {
		ret = tc956x_deassert_phy_reset(priv);
	}
	if (ret) {
		dev_err(priv->device, "Failed to deassert or assert QPS615 GPIO0%d\n",
			qpriv->phy_rst_gpio);
		if (regulator_disable(qpriv->phy_supply))
			dev_err(priv->device, "Failed to disable regulator\n");
	}

	dev_info(priv->device, "QPS615 PHY out of reset delay %d\n", qpriv->phy_rst_delay_us);
	usleep_range(qpriv->phy_rst_delay_us, qpriv->phy_rst_delay_us + 10);

	return ret;
}

static int tc956x_phy_power_off(struct tc956xmac_priv *priv)
{
	int ret = 0;
	struct tc956x_qcom_priv *qpriv = to_priv(priv);

	if (qpriv->phy_reverse_rst)
		ret = tc956x_deassert_phy_reset(priv);
	else
		ret = tc956x_assert_phy_reset(priv);

	if (ret) {
		dev_err(priv->device, "Failed to deassert or assert QPS615 GPIO%02d\n",
			qpriv->phy_rst_gpio);
		return ret;
	}

	ret = regulator_disable(qpriv->phy_supply);
	if (ret) {
		dev_err(priv->device,
			"Failed to disable PHY supply with error %d\n", ret);
		if (tc956x_deassert_phy_reset(priv))
			dev_err(priv->device, "Failed to deassert PHY\n");
	}

	return ret;
}

static int tc956x_platform_of_parse(struct device *dev,
				    struct tc956x_qcom_priv *qpriv)
{
	if (of_property_read_u32(dev->of_node, "qcom,phy-rst-gpio",
				 &qpriv->phy_rst_gpio)) {
		dev_err(dev, "Failed to get PHY reset GPIO\n");
		return -EINVAL;
	}

	if (of_property_read_u32(dev->of_node, "qcom,phy-rst-delay-us",
				 &qpriv->phy_rst_delay_us)) {
		dev_err(dev, "Failed to get PHY reset delay time\n");
		return -EINVAL;
	}
	qpriv->is_built_in_wol =
		of_property_read_bool(dev->of_node, "qcom,phy-built-in-wol");
	dev_info(dev, "is_built_in_wol %d\n", qpriv->is_built_in_wol);

	qpriv->phy_supply = devm_regulator_get(dev, "phy");
	if (IS_ERR(qpriv->phy_supply)) {
		dev_err(dev, "Failed to acquire supply 'phy-supply': %ld\n",
			PTR_ERR(qpriv->phy_supply));
		return -EINVAL;
	}
	if (!qpriv->is_built_in_wol) {
		qpriv->wol_irq = of_irq_get_byname(dev->of_node, "wol_irq");
		if (qpriv->wol_irq <= 0) {
			dev_info(dev, "wol_irq IRQ is set as %d\n",
				 qpriv->wol_irq);
		}
		qpriv->pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR_OR_NULL(qpriv->pinctrl)) {
			dev_err(dev, "Failed to get pinctrl handle\n");
			goto err_pinctrl_get;
		}

		qpriv->pinctrl_default = pinctrl_lookup_state(qpriv->pinctrl,
							      PINCTRL_STATE_DEFAULT);
		if (IS_ERR_OR_NULL(qpriv->pinctrl_default)) {
			dev_err(dev, "Failed to look up '%s' pinctrl state\n",
				PINCTRL_STATE_DEFAULT);
			goto err_pinctrl_lookup_state;
		}
	} else {
		qpriv->wol_irq = -1;
	}
	qpriv->phy_reverse_rst =
		of_property_read_bool(dev->of_node, "qcom,phy-reverse-rst");
	dev_info(dev, "phy reverse rst %d\n", qpriv->phy_reverse_rst);

	return 0;

err_pinctrl_lookup_state:
	devm_pinctrl_put(qpriv->pinctrl);
err_pinctrl_get:
	devm_regulator_put(qpriv->phy_supply);
	return -EINVAL;
}

int tc956x_platform_probe(struct tc956xmac_priv *priv,
			  struct tc956xmac_resources *res)
{
	int ret = 0;
	struct tc956x_qcom_priv *qpriv;

#ifndef TC956X_ETH0
	if (res->port_num == RM_PF0_ID)
		return ret;
#endif
#ifndef TC956X_ETH1
	if (res->port_num == RM_PF1_ID)
		return ret;
#endif

	dev_dbg(priv->device, "QPS615 platform probing has started\n");

	qpriv = kzalloc(sizeof(*qpriv), GFP_KERNEL);
	if (!qpriv)
		return -ENOMEM;

	priv->plat_priv = qpriv;

	ret = tc956x_platform_of_parse(priv->device, qpriv);
	if (ret) {
		dev_err(priv->device, "Failed to parse platform device tree\n");
		goto err_parse_properties;
	}

	if (qpriv->phy_reverse_rst)
		ret = tc956x_deassert_phy_reset(priv);
	else
		ret = tc956x_assert_phy_reset(priv);

	if (ret) {
		dev_err(priv->device,
			"Failed to assert the PHY reset with error %d\n", ret);
		goto err_assert_phy_rst;
	}

	if (!qpriv->is_built_in_wol) {
		ret = pinctrl_select_state(qpriv->pinctrl,
					   qpriv->pinctrl_default);
		if (ret) {
			dev_err(priv->device,
				"Failed to select the 'default' pincrl state\n");
			goto err_pinctrl_select_state;
		}
	}

	ret = tc956x_phy_power_on(priv);
	if (ret) {
		dev_err(priv->device, "Failed to power on PHY with error %d\n",
			ret);
		goto err_power_on;
	}

	if (!qpriv->is_built_in_wol && qpriv->wol_irq > 0)
		res->wol_irq = qpriv->wol_irq;

	dev_info(priv->device, "wol_irq IRQ is set as %d res->irq is %d\n",
		 res->wol_irq, res->irq);

	dev_info(priv->device,
		 "QPS615 platform probing has finished successfully\n");

	return 0;

err_power_on:
	irq_set_irq_wake(qpriv->wol_irq, 0);
err_pinctrl_select_state:
err_assert_phy_rst:
err_parse_properties:
	kfree(qpriv);
	priv->plat_priv = NULL;
	return -EINVAL;
}

int tc956x_platform_remove(struct tc956xmac_priv *priv)
{
	int ret = 0;
	struct tc956x_qcom_priv *qpriv = to_priv(priv);

#ifndef TC956X_ETH0
	if (priv->plat->port_num == RM_PF0_ID)
		return ret;
#endif
#ifndef TC956X_ETH1
	if (priv->plat->port_num == RM_PF1_ID)
		return ret;
#endif
	dev_dbg(priv->device, "Freeing QPS615 platform resources\n");

	ret = tc956x_phy_power_off(priv);
	if (ret)
		dev_err(priv->device, "Failed to power off PHY with error %d\n",
			ret);
	devm_regulator_put(qpriv->phy_supply);
	if (!qpriv->is_built_in_wol)
		devm_pinctrl_put(qpriv->pinctrl);

	kfree(priv->plat_priv);
	priv->plat_priv = NULL;

	return ret;
}

int tc956x_platform_suspend(struct tc956xmac_priv *priv)
{
	int ret = 0;
#ifndef TC956X_ETH0
	if (priv->plat->port_num == RM_PF0_ID)
		return ret;
#endif
#ifndef TC956X_ETH1
	if (priv->plat->port_num == RM_PF1_ID)
		return ret;
#endif

	if (priv->wolopts) {
		struct tc956x_qcom_priv *qpriv = to_priv(priv);

		if (qpriv->wol_irq > 0) {
			ret = enable_irq_wake(priv->wol_irq);
			if (unlikely(ret))
				dev_err(priv->device,
					"Failed to enable WOL IRQ %d with error %d\n",
					priv->wol_irq, ret);
		}

	} else {
		ret = tc956x_phy_power_off(priv);
		if (ret)
			dev_err(priv->device,
				"Failed to power off PHY with error %d\n", ret);
	}

	return ret;
}

int tc956x_platform_resume(struct tc956xmac_priv *priv)
{
	int ret = 0;
#ifndef TC956X_ETH0
	if (priv->plat->port_num == RM_PF0_ID)
		return ret;
#endif
#ifndef TC956X_ETH1
	if (priv->plat->port_num == RM_PF1_ID)
		return ret;
#endif
	if (priv->wolopts) {
		struct tc956x_qcom_priv *qpriv = to_priv(priv);

		if (qpriv->wol_irq > 0) {
			ret = disable_irq_wake(priv->wol_irq);
			if (unlikely(ret))
				dev_err(priv->device,
					"Failed to disable WOL IRQ %d with error %d\n",
					priv->wol_irq, ret);
		}
	} else {
		ret = tc956x_phy_power_on(priv);
		if (ret)
			dev_err(priv->device,
				"Failed to power on the PHY with error %d\n",
				ret);
	}

	return ret;
}

int tc956x_platform_port_interface_overlay(struct device *dev, struct tc956xmac_resources *res)
{
	u32 interface;

	if (of_property_read_u32(dev->of_node, "qcom,phy-port-interface",
				 &interface)) {
		dev_err(dev, "Failed to get phy port interface\n");
	} else {
		dev_info(dev, "phy port interface overlay to %d from %d\n",
			 interface, res->port_interface);
		res->port_interface = interface;
	}
	return 0;
}
