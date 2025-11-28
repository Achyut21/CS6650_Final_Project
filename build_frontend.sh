#!/bin/bash
cd "$(dirname "$0")/frontend"
npm run build
rm -rf ../gateway/public/*
cp -r dist/* ../gateway/public/
echo "Done!"
