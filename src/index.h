const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<style>
.card{
    max-width: 400px;
     min-height: 250px;
     background: #02b875;
     padding: 30px;
     box-sizing: border-box;
     color: #FFF;
     margin:20px;
     box-shadow: 0px 2px 18px -4px rgba(0,0,0,0.75);
}
</style>
<body>
<div class="card">
   <h4>modbus</h4><br>
   <p>TIME: <span id="time">0</span></p><br>
   <p>CO2: <span id="co2Value">0</span> ppm</p><br>
   <p>TEMP: <span id="tempValue">0</span> &#176;C</p><br>
   <p>RH: <span id="rhValue">0</span> %</p><br>
</div>
<script>

setInterval(function() {
  // Call a function repetatively with 5 Second interval
  getData();
}, 5000); //2000mSeconds update rate

setInterval(function() {
  // Call a function repetatively with 1 Second interval
  getTime();
}, 1000);

function getData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("co2Value").innerHTML =
      this.responseText.substring(0,3);
      document.getElementById("tempValue").innerHTML =
      this.responseText.substring(4,9);
      document.getElementById("rhValue").innerHTML =
      this.responseText.substring(10,15);
    }
  };
  xhttp.open("GET", "modbus", true);
  xhttp.send();
}
function getTime() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("time").innerHTML =
      this.responseText;
    }
  };
  xhttp.open("GET", "time", true);
  xhttp.send();
}
</script>
</body>
</html>
)=====";