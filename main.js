//Cluj coordinates
var lat = 46.770439;
var lng = 23.591423;

var map = L.map('map').setView([lat, lng], 12);//center the map on the specified coordiantes

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png?{foo}', {foo: 'bar', attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'}).addTo(map);

getData();
async function getData() {
    const response = await fetch('data.db');
    const data = await response.text();//extracting the data from the database

    // var pointMarker = L.marker([lat, lng]).addTo(map);
    // pointMarker.bindPopup(String(pointMarker.getLatLng()));

    const table = data.split('\n');//split the data into rows

    var length = table.length;//storing the total number of locations
    for(let i = 0; i < length; i++)//extracting the latitude and longitude for the every location
    {
        var matches = table[i].match(/\d+/g);
        var a = matches[0]+'.'+matches[1];
        var b = matches[2]+'.'+matches[3];
        var latitude = parseFloat(a);
        var longitude = parseFloat(b);
        L.marker([latitude, longitude]).addTo(map).bindPopup('Latitude:'+latitude+'<br>Longitude:'+longitude);
    }
}
