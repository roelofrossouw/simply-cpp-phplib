"""A stand-in for ai-api's /faces route, for SC\\Face's test. Usage: ai_api.py TOKEN
Listens on a free port and says which on stdout ("listening PORT"). POST /faces with
"Authorization: Bearer TOKEN" answers as ai-api does (a JSON list: version, features, box):

  body "no faces"       []
  body "not a picture"  422 {"error": ...}
  any other body        two faces: a small one (box 10 x 10, features all 0.1) and a large one
                        (box 100 x 120, features e0 = [1, 0, 0, ...]), the small one first;
                        at most max_faces of them
  a wrong token         401
"""
import http.server, json, sys, threading, urllib.parse

token = sys.argv[1]


def face(features, left, top, width, height):
    return {"version": 1, "features": features, "box": {"left": left, "top": top, "width": width, "height": height}}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def send(self, status, answer):
        body = json.dumps(answer).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        url = urllib.parse.urlsplit(self.path)
        body = self.rfile.read(int(self.headers.get("Content-Length") or 0))
        if url.path != "/faces":
            return self.send(404, {"error": "no such route"})
        if self.headers.get("Authorization") != "Bearer " + token:
            return self.send(401, {"error": "unauthorized"})
        if body == b"not a picture":
            return self.send(422, {"error": "not a picture"})
        if body == b"no faces":
            return self.send(200, [])
        query = urllib.parse.parse_qs(url.query)
        max_faces = int(query.get("max_faces", ["5"])[0])
        large = [0.0] * 512
        large[0] = 1.0
        faces = [face([0.1] * 512, 5.0, 5.0, 10.0, 10.0), face(large, 50.0, 40.0, 100.0, 120.0)]
        return self.send(200, faces[:max_faces])


server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
print("listening %d" % server.server_address[1], flush=True)
threading.Thread(target=server.serve_forever, daemon=True).start()
sys.stdin.read()  # until the test closes it
