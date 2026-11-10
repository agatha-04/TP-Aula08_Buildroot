HCSR04_DRIVER_VERSION = 1.0
HCSR04_DRIVER_SITE = $(BR2_EXTERNAL_TP9_PATH)/package/hcsr04-driver/src
HCSR04_DRIVER_SITE_METHOD = local
$(eval $(kernel-module))
$(eval $(generic-package))
