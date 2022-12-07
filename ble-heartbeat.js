/*
  BLE Heartbeat Sensor with Browser Interface
  Build up on sample code by 
  – Jeremy Ellis (https://forum.arduino.cc/t/html-connect-to-ble-using-p5-ble-js-and-p5-js/629754)
  – Elias Santistevan, Sparkfun Electronics 
    (https://learn.sparkfun.com/tutorials/sparkfun-pulse-oximeter-and-heart-rate-monitor-hookup-guide#hardware-hookup) 
*/



// Global variables and constants

// The serviceUuid must match the serviceUuid of the device you would like to connect
const serviceUuid = "465a640e-7553-5ddc-86c9-b3e59919c36d";
let nanoBLE;


function setup() {
  nanoBLE = new p5ble();
}

window.addEventListener('DOMContentLoaded', (event) => {
    document.querySelector('#bleConnect').addEventListener('click', () => {	nanoBLE.connect(serviceUuid, gotCharacteristics) });
});


function gotCharacteristics(error, characteristics) {
	if (error) console.log('error: ', error);
	document.body.classList.add('ble-connected');
	
	// addEventListeners to notifying characteristics
	characteristics.forEach(ch =>{
		if(ch.properties.notify) {
			if(ch.uuid == "e7ea0fb5-4f46-4241-8825-a3fa60acbb71") nanoBLE.startNotifications(ch, handleStatusNotification);
			else if(ch.uuid == "6e4a51f1-859d-4323-b984-0e8968fc8ade") nanoBLE.startNotifications(ch, handleHeartrateNotification);
		}
		
	});
}

// status code explanation from SparkFun sensor
const statusArray = {
	0: "", 
	1: "Connecting ...", 
	255: "Measuring ...", 
	254: "Keep sensor still", 
	253: "Place finger on sensor", 
	252: "Reduce pressure on sensor", 
	251: "You sure that's a finger?", 
	250: "Keep finger still"
};

function handleStatusNotification(status) {
	document.querySelector('#status').innerHTML = statusArray[status];
	if(status != 0) document.querySelector('#rate').innerHTML = '&dash;&dash;';
}
	
function handleHeartrateNotification(data) {
	if(data && data >0) document.querySelector('#rate').innerHTML = data;
	else document.querySelector('#rate').innerHTML = '&dash;&dash;';
}