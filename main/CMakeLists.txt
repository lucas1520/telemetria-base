idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES
                        ble
                        sd_card
                        serial
                        telemetry
                        esp_driver_uart
                        esp_driver_gpio
                    )
