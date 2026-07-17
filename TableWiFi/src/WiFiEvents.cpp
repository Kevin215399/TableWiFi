/*#include "TableLED_WiFi.h"

String WiFiEventsJS[] = R"RAW(
    async function Run(){
        try {
        const response = await fetch("http://Desk.local/update?message=hi");
        if (!response.ok) {
            throw new Error(`Response status: ${response.status}`);
        }

        const result = await response;
        console.log(result);
        } catch (error){
        console.log(error);
        }
    }

    if (!!window.EventSource) {
        var source = new EventSource('/events');
        source.addEventListener('message', function(e) {
        console.log("message", e.data);
        }, false);
    }
)RAW";*/
