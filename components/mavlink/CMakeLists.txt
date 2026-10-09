idf_component_register(INCLUDE_DIRS "c_library_v2")

# Os headers gerados do MAVLink pegam endereço de campos packed
target_compile_options(${COMPONENT_LIB} INTERFACE -Wno-address-of-packed-member)
