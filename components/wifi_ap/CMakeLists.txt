idf_component_register(SRCS "wifi_ap.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        esp_wifi
                        esp_netif
                        esp_event
                        esp_http_server
                        telemetry
                    )
