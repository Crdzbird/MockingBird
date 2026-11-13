#!/usr/bin/env bash
# Chrome with a synthetic microphone and audio output allowed without a
# user gesture, for the mockingbird_web capture and player tests:
#   CHROME_EXECUTABLE=scripts/chrome_fake_mic.sh flutter test --platform chrome
exec "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  --use-fake-ui-for-media-stream \
  --use-fake-device-for-media-stream \
  --autoplay-policy=no-user-gesture-required \
  "$@"
