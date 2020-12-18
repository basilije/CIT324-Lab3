/***********************************************************************************
* Project: Lab3 - Where in the world is TTGO T-Beam-o?
* Class: CIT324 - Networking for IoT
* Author: Kory Herzinger
*
* File: gps-html.h
* Description: Defines the HTML markup for the GPS web server.
* Date: 09/06/2020
**********************************************************************************/
#pragma once
const char GPS_APP_HTML[] =
    "<html>"    
        "<head>"
            "<title>CIT324 GPS Application</title>"
                "<script>"
                    "function getLocation(callback) {"
                        "var url = '/location';"
                        "var xhttp = new XMLHttpRequest();"
                        "xhttp.onreadystatechange = function() {"
                            "if (this.readyState == 4 && this.status == 200) {"
                                "callback(this);"
                            "}"
                        "};"
                        "xhttp.open('GET', url, true);"
                        "xhttp.send();"
                    "}"
                    "function start() {"
                        "setInterval(function(){"
                            "getLocation(function(xhttp) {"
                                "var location = JSON.parse(xhttp.responseText);"
                                "var lat = document.getElementById('lat');"
                                "var long = document.getElementById('long');"
                                "lat.innerHTML = location.gps.lat;"
                                "long.innerHTML = location.gps.long;"
                            "})"
                        "}, 5000);"
                    "}"
                "</script>"
        "</head>"
        "<body onload=\"start()\">"
            "<h1>CIT324 GPS Application</h1>"
            "<h2>Latitude: <span id=\"lat\">latitude_Unknown</span></h2>"
            "<h2>Longitude: <span id=\"long\">longitude_Unknown</span></h2>"
        "</body>"
    "</html>";