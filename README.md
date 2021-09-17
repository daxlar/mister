# mister

![mister device](/imgs/mister.jpeg)

#### What is this ?

This the starting point to all the repositories in my github that start with the `mister` prefix. You can find them here:

1. [mister-frontend](https://github.com/daxlar/mister-frontend)
2. [mister-backend](https://github.com/daxlar/mister-backend)

This repository, and the above 2 combine to create a protoype solution for prospectively helping workplaces ease back into in-person meetings during the COVID-19 pandemic.

The `mister` project is a singular device that tries to intelligently disinfect meeting rooms after they are used by spraying disinfectant in a mist-like fashion in 360 degrees, hence the name `mist` + `er`. Each device is to be uniquely assigned to a meeting room. Along with repositories listed above, the system behaves as following:

1. `mister-frontend` displays meeting parameters that the user can edit to check the availability of meeting intervals in different rooms. Once the parameters are chosen, `mister-backend` takes a look at a DynamoDB table that stores selected meeting intervals, and returns available meeting intervals in their available rooms to `mister-frontend`.
2. `mister-frontend` displays the available meeting intervals in different rooms, and the user can now choose a desired meeting interval. `mister-backend` stores this meeting interval in the DynamoDB table. Based on the room selected, the device shadow in AWS IoT Core corresponding to that device is updated with the meeting interval. Since this project was developed with one device, that device was hardcoded to correspond to room 1.
3. `mister` receives updates from its device shadow and marks the meeting interval down in memory, in 15 minute block intervals. Since the meeting intervals should(to do...) only be in contiguous blocks of 15 minutes (for simplicity), a 2 hour meeting would be 8 blocks worth of intervals in `mister` memory. Every 15 minutes, `mister` checks to see if a meeting had just occurred, and one had just occurred and there is not meeting currently active, then it will activate.
4. First, the PIR is checked every 2 minutes to see if the meeting had spilled over and there are still workers in the meeting room. If there are, it will check every 2 minutes until the supposedly empty 15 minute interval is done. If the room was used for the complete 15 minute duration, then the current 15 minute meeting interval will be marked as if a meeting had occurred.
5. If the PIR does not detect anyone, `mister` will emit a tone and a screen with a confirm button that counts down from 30 seconds starts. If someone confirms, the system will loop back to step 4.
6. If the confirm button is not pressed, then the system will activate, spraying a fine mist in 360 degrees. Once the misting finishes, it will loop back to step 3.

#### Getting started

1. Get a [core2ForAWS](https://www.amazon.com/M5Stack-Core2-ESP32-Development-EduKit/dp/B08VGRZYJR)
2. Clone this repository
3. [Setting up the environment](https://edukit.workshop.aws/en/getting-started/prerequisites.html)
4. [Provisioning part 1](https://edukit.workshop.aws/en/blinky-hello-world/prerequisites.html)
5. [Provisioning part 2](https://edukit.workshop.aws/en/blinky-hello-world/device-provisioning.html)
6. [Connecting to AWS IoT Core](https://edukit.workshop.aws/en/blinky-hello-world/connecting-to-aws.html)

Since this is a project with hardware and some physical components, follow along with this link on hackster to learn more:
