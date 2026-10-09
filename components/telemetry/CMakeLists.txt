idf_component_register(SRCS "telemetry_mock.c" "telemetry_store.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_timer
                    )
