# stop script on error
set -e

# Check to see if root CA file exists, download if not
if [ ! -f ./root-CA.crt ]; then
  printf "\nDownloading AWS IoT Root CA certificate from AWS...\n"
  curl https://www.amazontrust.com/repository/AmazonRootCA1.pem > root-CA.crt
fi

# install AWS Device SDK for NodeJS if not already installed
if [ ! -d ./node_modules ]; then
  printf "\nInstalling AWS SDK...\n"
  npm install aws-iot-device-sdk
fi

# run pub/sub sample app using certificates downloaded in package
printf "\nRunning pub/sub sample application...\n"
node node_modules/aws-iot-device-sdk/examples/device-example.js --host-name=a73pyqfgfpqv5-ats.iot.us-west-2.amazonaws.com --private-key=ESP32OLED.private.key --client-certificate=ESP32OLED.cert.pem --ca-certificate=root-CA.crt --client-id=sdk-nodejs-7976cd64-1fca-405e-a1d8-b6ae69350f7a