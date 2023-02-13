# modbus_rtu  
Modbus RTU with website using Webserver and ArduinoJson(rustful api and json).  
API: /data
### Using Postmon to request with JSON:  
{ "slave_id": "02", "func": "03", "reg_addr": "44", "read_count": "03" }  
### Using curl Command  
curl -X POST -H "Content-Type: application/json" -d '{"slave_id":"02","func":"03","reg_addr":"0044","read_count":"0003"}' 192.168.1.69/data
### You will get the Response:   
{"Device":"co2Meter","timeStamp":"15:47:18","modbusRead":[{"type":"CO2","value":"809","unit":"ppm"},{"type":"TEMP","value":"24.43","unit":"°C"},{"type":"RH","value":"50.74","unit":"%"}]}