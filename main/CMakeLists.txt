set(COMPONENT_SRCS "main.c" "ui.c" "wifi.c" "appRTC.c" "iot.c" "pir.c" "servo.c")
set(COMPONENT_ADD_INCLUDEDIRS "." "./includes")

register_component()

target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)