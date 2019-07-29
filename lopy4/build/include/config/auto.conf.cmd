deps_config := \
	/home/maurice/esp/esp-idf/components/app_trace/Kconfig \
	/home/maurice/esp/esp-idf/components/aws_iot/Kconfig \
	/home/maurice/esp/esp-idf/components/bt/Kconfig \
	/home/maurice/esp/esp-idf/components/driver/Kconfig \
	/home/maurice/esp/esp-idf/components/esp32/Kconfig \
	/home/maurice/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/maurice/esp/esp-idf/components/esp_event/Kconfig \
	/home/maurice/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/maurice/esp/esp-idf/components/esp_http_server/Kconfig \
	/home/maurice/esp/esp-idf/components/ethernet/Kconfig \
	/home/maurice/esp/esp-idf/components/fatfs/Kconfig \
	/home/maurice/esp/esp-idf/components/freemodbus/Kconfig \
	/home/maurice/esp/esp-idf/components/freertos/Kconfig \
	/home/maurice/esp/esp-idf/components/heap/Kconfig \
	/home/maurice/esp/esp-idf/components/libsodium/Kconfig \
	/home/maurice/esp/esp-idf/components/log/Kconfig \
	/home/maurice/esp/esp-idf/components/lwip/Kconfig \
	/home/maurice/esp/esp-idf/components/mbedtls/Kconfig \
	/home/maurice/esp/esp-idf/components/mdns/Kconfig \
	/home/maurice/esp/esp-idf/components/mqtt/Kconfig \
	/home/maurice/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/maurice/esp/esp-idf/components/openssl/Kconfig \
	/home/maurice/esp/esp-idf/components/pthread/Kconfig \
	/home/maurice/esp/esp-idf/components/spi_flash/Kconfig \
	/home/maurice/esp/esp-idf/components/spiffs/Kconfig \
	/home/maurice/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/maurice/esp/esp-idf/components/vfs/Kconfig \
	/home/maurice/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/maurice/esp/lopy4/components/arduino/Kconfig.projbuild \
	/home/maurice/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/maurice/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/maurice/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/maurice/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
