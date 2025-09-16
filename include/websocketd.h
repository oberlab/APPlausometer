#pragma once

// websocket broadcast interval (ms) for smooth meter updates
const unsigned long websocket_update_interval = 100;

void setup_websocketd();
void loop_websocketd();
void websocket_update();
