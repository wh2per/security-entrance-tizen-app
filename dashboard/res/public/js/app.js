window.onload = function(){
	var canvas;
	var frame_timestamp = new Array(100);
	var request_time;
	var frame_number = 0;
	
	var xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function() {
		var previewElement = document.getElementById('camera-view');
		
		if (xhr.readyState === 4) {
			previewElement.src = xhr.response;
		}
	};
	
	canvas = new Canvas("camera-view-canvas");
	
	runWebSocket();
	
	function update_fps() {
		frame_timestamp[frame_number] = Date.now();
		var fpsTag = document.getElementById('fps');
        if (fpsTag != null)
            fpsTag.innerHTML = JSON.stringify(fps());
        if (++frame_number == frame_timestamp.length) frame_number = 0;
	}
	
	function fps() {
        var i = frame_number;
        var now = Date.now();
        do { // backtrace to find the frame 1000 milliseconds before
            if (now - frame_timestamp[i] >= 1000) {
                var fps = frame_number - i;
                if (fps < 0)
                    fps += frame_timestamp.length;
                return fps;
            }
            if (--i < 0) // wrap around to the last slot in the array
                i = frame_timestamp.length - 1;
        } while (i != frame_number)
    }
	
	function runWebSocket() {
		var wsUri = "ws://" + window.location.hostname + ":8888/";
		
		websocket = new WebSocket(wsUri);
		websocket.onopen = function(evt) { onOpen(evt) };
		websocket.onclose = function(evt) { onClose(evt) };
		websocket.onmessage = function(evt) { onMessage(evt) };
		websocket.onerror = function(evt) { onError(evt) };
	}
	
	function onOpen(evt) {
		writeToScreen("[+] Connected");
		doSend("Hello from fucking browser via webSocket");
	}
	function onClose(evt) {
		writeToScreen("[+] Disconnected");
	}
	function onMessage(evt) {
		var urlCreator = window.URL || window.webkitURL;
		var imageUrl = urlCreator.createObjectURL(evt.data);
		document.querySelector("#camera-view").src = imageUrl;
		//update view HERE
		update_fps();
		
		var arrayBuffer;
//		var fileReader = new FileReader();
//		fileReader.onload = (event) => {
//			arrayBuffer = event.target.result;
//			var exif = EXIF.readFromBinaryFile(arrayBuffer);
//			var exifInfoString = asciiToStr(exif.UserComment, 8);
//			var type = 'blur';
//			if (getResultType(exifInfoString) != 0) {
//				type = 'active';
//			}
//		};
//		
//		fileReader.readAsArrayBuffer(evt.data);
		doSend("ack", true);
	}
	
	function onError(evt) {
		writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
	}
	
	function doSend(message, dont_print_log) {
		websocket.send(message);
		if (!dont_print_log)
			writeToScreen("[+] SENT: " + message);
	}
	
	function writeToScreen(message) {
		var output = document.getElementById("output");
		if (output == null)
			return;
		var pre = document.createElement("p");
		pre.style.wordWrap = "break-work";
		pre.innerHTML = message;
		output.appendChild(pre);
	}
	
	function asciiToStr(asciiArr, start) {
		var string = "";
		var i = start;
		for (; i< asciiArr.length; i++){
			String += String.fromCharCode(asciiArr[i]);
		}
		return string;
	}
};

function Canvas(canvasId) {
	this.viewCanvas = document.getElementById(canvasId);
	this.viewContext = this.viewCanvas.getContext("2d");
}

function imgChange(binaryData){
    var c = document.getElementById("camera-view");
    var ctx = c.getContext("2d");
    var img = new Image();
    img.onload = function() {
        ctx.drawImage(img);
    };
    
    img.onerror = function(e) {
        document.getElementById("demo").innerHTML = "Error: " + e;
    };
    img.src = binaryToDataURL(binaryData);
}

function hexToBase64(str) {
    return btoa(String.fromCharCode.apply(null, str.replace(/\r|\n/g, "").replace(/([\da-fA-F]{2}) ?/g, "0x$1 ").replace(/ +$/, "").split(" ")));
}