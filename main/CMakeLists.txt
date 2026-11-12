idf_component_register(
    SRCS "app_main.c" "app_ao.c" "app_ui.c" "badge_class.c" "badge_log.c" "badge_scan.c"
    INCLUDE_DIRS "."
    REQUIRES kode
    PRIV_REQUIRES kscreen
)
