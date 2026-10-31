"""Integration constants."""
DOMAIN = "evohome_dashboard"
PLATFORMS = ("climate", "sensor")
DEFAULT_INTERVAL = 180
MIN_INTERVAL = 60
MAX_ERRORS = 80
PANEL_PATH = "evohome-dashboard"

# Optional read-only RAMSES RF room-demand observation.
CONF_RF_CONTROLLER_ID = "rf_controller_id"
CONF_RF_MQTT_TOPIC = "rf_mqtt_topic"
CONF_RF_ZONE_NAMES = "rf_zone_names"
DEFAULT_RF_MQTT_TOPIC = "RAMSES/GATEWAY/+/rx"
