idf_component_register(SRCS "ble.c" "gap.c" "gatt_svc.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        telemetry
                        bt
                        nvs_flash
                    )
