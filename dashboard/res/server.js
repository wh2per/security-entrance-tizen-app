 /*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var fs = require('fs');
var http = require('http');

var SERVER_ROOT_FOLDER_PATH = '/opt/usr/globalapps/org.tizen.smart-surveillance-camera.dashboard/res/';
var LATEST_FRAME_FILE_PATH = '/home/owner/apps_rw/org.example.security-entrance-tizen-app/shared/data/latest.jpg'

function extractPath(url) {
  var urlParts = url.split('/'),
    i = 0,
    l = urlParts.length,
    result = [];
  for (; i < l; ++i) {
    if (urlParts[i].length > 0) {
      result.push(urlParts[i]);
    }
  }
  return result;
}

http.createServer(function(req, res) {
  req.on('end', function() {
    var path = extractPath(req.url);
    console.log(req.url)
    // var last = path[path.length - 1];
    if (path[0] === undefined) {
      res.writeHead(200);
      res.end(fs.readFileSync(SERVER_ROOT_FOLDER_PATH + 'public/index.html'));
    } else if (path[0] == 'test') {
      res.writeHead(200);
      res.end(fs.readFileSync(SERVER_ROOT_FOLDER_PATH + 'public/test.html'));
    } else if (req.url == '/js/app.js') {
      res.writeHead(200);
      res.end(fs.readFileSync(SERVER_ROOT_FOLDER_PATH + 'public/js/app.js'));
    } else if (req.url == '/css/style.css') {
      res.writeHead(200);
      res.end(fs.readFileSync(SERVER_ROOT_FOLDER_PATH + 'public/css/style.css'));
    } else {
      res.setHeader('Location', 'http://download.tizen.online/smart-surveillance' + req.url);
      res.writeHead(302);
      res.end();
      console.log('33333');
    }
  });
}).listen(9090);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

var ENABLE_WEBSOCKET = true;

if (ENABLE_WEBSOCKET) {

  var websocket = require('websocket');

  var options = {
    port: 8888
  }

  var server = new websocket.Server(options, Listener);

  function Listener(ws) {
    console.log('Client connected: handshake done!');
    ws.ack = true;
    ws.on('message', function (msg) {
      console.log('Message received: %s', msg.toString());
      // ws.send(msg.toString(), {mask: true, binary: false}); //echo
      // ws.send('Received: ' + msg.toString()); //echo
      // server.close();
      ws.ack = true;
    });
    ws.on('ping', function (msg) {
      console.log('Ping received: %s', msg.toString());
    });
    ws.on('error', function (msg) {
      console.log('Error: %s', msg.toString());
    });

    var i = 0;
    var prev = 0;
    var timeout = setInterval(function() {
      if (!ws.ack)
        return false;
      var now = Date.now();
      var data;
      try {
       data = fs.readFileSync(LATEST_FRAME_FILE_PATH);
      } catch (err) {
    	console.log(err);
        data = fs.readFileSync(SERVER_ROOT_FOLDER_PATH + 'default.gif');
      }
      ws.send(data, {mask: false, binary: true});
      console.log(`Sending frame(${i++}), interval(${now - prev} ms)`);
      // server.broadcast(data, {mask: false, binary: true});
      // server.broadcast(`HELLO TO ALL FROM IoT.js!!! (${i++}, ${now - prev})`);
      prev = now;
      ws.ack = false;
    }, 1000 / 15.0);

    ws.on('close', function (msg) {
      console.log('Client close: ' + msg.reason + ' (' + msg.code + ')');
      clearInterval(timeout);
    });
  }

  server.on('error', function (msg) {
    console.log('Error: %s', msg.toString());
  });

  server.on('close', function (msg) {
    console.log('Server close: ' + msg.reason + ' (' + msg.code + ')');
  });

}