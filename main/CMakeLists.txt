set(COMPONENT_SRCS "main.c" "ui.c" "wifi.c" "iot.c" "appRTC.c" "pir.c" "servo.c" "meeting.c" "sound.c" "music.c" "robot.c" "mister.c")
set(COMPONENT_ADD_INCLUDEDIRS "." "./includes")

register_component()

target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)