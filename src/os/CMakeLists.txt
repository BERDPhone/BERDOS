add_executable(boot
    boot.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(boot pico_stdlib)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(boot)