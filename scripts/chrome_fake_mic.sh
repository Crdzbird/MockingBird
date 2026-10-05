#!/usr/bin/env bash
# Chrome with a synthetic microphone, for the mockingbird_web capture tests:
#   CHROME_EXECUTABLE=scripts/chrome_fake_mic.sh flutter test --platform chrome
exec "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  --use-fake-ui-for-media-stream \
  --use-fake-device-for-media-stream \
  "$@"
