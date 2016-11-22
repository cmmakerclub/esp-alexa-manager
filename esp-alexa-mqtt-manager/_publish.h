#include <MqttConnector.h>

extern float t_dht;
extern float h_dht;
extern const char* DEVICE_NAME;

MqttConnector::prepare_data_hook_t on_prepare_data =
[&](JsonObject *root) -> void {

  JsonObject& data = (*root)["d"];
  JsonObject& info = (*root)["info"];

  weight = map(analogRead(A0), 500, 725,0, 515);
  
  data["myName"] = DEVICE_NAME;
  data["tags"] = "inside,cmmc";
  data["weight"] = weight;

  //    data["temp"] = t_dht;
  //    data["humid"] = h_dht;
};
