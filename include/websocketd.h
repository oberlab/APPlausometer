#pragma once


// websocket broadcast interval (ms) for smooth meter updates
const unsigned long websocket_update_interval = 100;

void setup_websocketd();
void loop_websocketd();
void ws_update_livedata();
void ws_update_storedrecords();
void ws_update_config();

