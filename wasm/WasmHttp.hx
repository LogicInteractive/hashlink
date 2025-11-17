// Wrapper for WASM HTTP support using browser XMLHttpRequest
class WasmHttp {
    var url:String;
    var method:String = "GET";
    var headers:Map<String, String> = new Map();
    var postData:String = null;

    public var onData:String->Void;
    public var onError:String->Void;
    public var onStatus:Int->Void;

    public function new(url:String) {
        this.url = url;
    }

    public function setHeader(name:String, value:String) {
        headers.set(name, value);
    }

    public function setPostData(data:String) {
        this.postData = data;
        this.method = "POST";
    }

    public function request(?post:Bool) {
        if (post == true) {
            method = "POST";
        }

        // Build headers string (one per line)
        var headersStr = "";
        for (key in headers.keys()) {
            headersStr += key + ": " + headers.get(key) + "\n";
        }

        // Call native HTTP function
        #if hl_emscripten
        _httpRequest(method, url, headersStr, postData, onData, onError, onStatus);
        #else
        if (onError != null) {
            onError("HTTP not supported on this platform");
        }
        #end
    }

    @:hlNative("std", "http_request")
    static function _httpRequest(
        method:String,
        url:String,
        headers:String,
        body:String,
        onData:String->Void,
        onError:String->Void,
        onStatus:Int->Void
    ):Int {
        return 0;
    }
}
