idf_component_register(SRCS "serial.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        esp_driver_uart
                        esp_driver_gpio
                        driver
                        telemetry
                    )
