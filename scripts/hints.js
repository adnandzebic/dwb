var dwbWordRegexp = new RegExp("^[\w\u00c0-\u024f]*$");
String.prototype.isInt = function() { return !isNaN(parseInt(this)); }
String.prototype.isUpper = function() { return this == this.toUpperCase() && dwbWordRegexp.test(this); }
String.prototype.isLower = function() { return this == this.toLowerCase(); }
DwbHintObj = (function() {
  _letterSeq = "FDSARTGBVECWXQYIOPMNHZULKJ";
  _font = "bold 10px monospace";
  _style = "letter";
  _fgColor = "#ffffff";
  _bgColor = "#000088";
  _activeColor = "#00ff00";
  _normalColor = "#ffff99";
  _hintBorder = "2px dashed #000000";
  _hintOpacity = 0.65;
  _elements = [];
  _activeArr = [];
  _lastInput = null;
  _lastPosition = 0;
  _activeInput = null;
  _styles = 0;
  _active = null;
  _markHints = false;
  _bigFont = null;
  _actualType = 0;
  _hintTypes = [ "a, map, textarea, select, input:not([type=hidden]), button,  frame, iframe, *[onclick], *[onmousedown]",  // HINT_T_ALL
                 "a",  // HINT_T_LINKS
                 "img",  // HINT_T_IMAGES
                 "input[type=text], input[type=password], input[type=search], textarea",  // HINT_T_EDITABLE
               ];
  const HintTypes = {
    HINT_T_ALL : 0,
    HINT_T_LINKS : 1,
    HINT_T_IMAGES : 2,
    HINT_T_EDITABLE : 3,
  };

  const __newHint = function(element, win, rect) {
    this.element = element;
    this.overlay = null;
    this.win = win;
    this.textDesc = null;
    var leftpos, toppos;
    var scrollY = win.scrollY;
    var scrollX = win.scrollX;
    var hint = __createElement("div");
    var toppos = rect.top + scrollY;
    var leftpos = rect.left + scrollX;
    var t = Math.max(toppos, scrollY);
    var l = Math.max(leftpos, scrollX);
    hint.style.top = t + "px";
    hint.style.left = l + "px";

    hint.className =  "dwb_hint";
    this.createOverlay = function() {
      var w, h;
      var comptop = toppos - scrollY;
      var compleft = leftpos - scrollX;
      var height = rect.height;
      var width = rect.width;
      h = comptop > 0 ? height : height + comptop;
      overlay = __createElement("div");
      overlay.className = "dwb_overlay_normal";
      overlay.style.width = (compleft > 0 ? width : width + compleft) + "px";
      overlay.style.height = (comptop > 0 ? height : height + comptop) + "px";
      overlay.style.top = t + "px";
      overlay.style.left = l + "px";
      overlay.style.display = "block";
      overlay.style.cursor = "pointer";
      overlay.onclick = function() { __evaluate(element); };
      this.overlay = overlay;
    }
    this.hint = hint;
    if (! (element instanceof HTMLInputElement && element.hasAttribute("id"))) 
      return;
    var label = win.document.body.querySelector("label[for='" + element.id + "']");
    if (label == null) 
      return;
    var text = __createElement("div");
    text.style.font = "normal 8px monospace";
    text.innerText = " : " + label.innerText;
    text.style.display = "inline";
    this.hint.appendChild(text);
    this.textDesc = text;
    
  }
  const __createElement = function(tagname) {
    element = document.createElement(tagname);
    if (!element.style) {
      var namespace = document.getElementsByTagName('html')[0].getAttribute('xmlns') || "http://www.w3.org/1999/xhtml";
      element = document.createElementNS(namespace, tagname);
    }
    return element;
  }
  const __numberHint = function (element, win, rect) {
    this.constructor = __newHint;
    this.constructor(element, win, rect);
    var topobj = this;

    this.getTextHint = function (i, length) {
      var e = this.element;
      start = length <=10 ? 1 : length <= 100 ? 10 : 100;
      var content = document.createTextNode(start + i);
      if (!this.textDesc) 
        this.hint.appendChild(content);
      else 
        this.hint.insertBefore(content, this.textDesc);
    }

    this.betterMatch = function(input) {
      if (input.isInt()) {
        return 0;
      }
      var bestposition = 37;
      var ret = 0;
      for (var i=0; i<_activeArr.length; i++) {
        var e = _activeArr[i];
        if (input && bestposition != 0) {
          var content = e.element.textContent.toLowerCase().split(" ");
          for (var cl=0; cl<content.length; cl++) {
            if (content[cl].toLowerCase().indexOf(input) == 0) {
              if (cl < bestposition) {
                ret = i;
                bestposition = cl;
                break;
              }
            }
          }
        }
      }
      return ret;
    }
    this.matchText = function(input) {
      var ret = false;
      if (input.isInt()) {
        var regEx = new RegExp('^' + input);
        return regEx.test(this.hint.textContent);
      }
      else {
        var inArr = input.split(" ");
        for (var i=0; i<inArr.length; i++) {
          if (!this.element.textContent.toLowerCase().match(inArr[i].toLowerCase())) {
            return false;
          }
        }
        return true;
      }
    }
  };
  const __letterHint = function (element, win, rect) {
    this.constructor = __newHint;
    this.constructor(element, win, rect);

    this.betterMatch = function(input) {
      return 0;
    }
    this.getTextHint = function(i, length) {
      var text;
      var l = _letterSeq.length;
      if (length < l) {
        text = _letterSeq[i];
      }
      else if (length < 2*l) {
        var rem = (length) % l;
        var sqrt = Math.sqrt(2*rem);
        var r = sqrt == (getint = parseInt(sqrt)) ? sqrt + 1 : getint;
        if (i < l-r) {
          text = _letterSeq[i];
        }
        else {
          var newrem = i%(r*r);
          text = _letterSeq[Math.floor( (newrem / r) + l - r )] + _letterSeq[l-newrem%r - 1];
        }
      }
      else {
        text = _letterSeq[i%l] + _letterSeq[l - 1 - (parseInt(i/l))];
      }
      var content = document.createTextNode(text);
      if (!this.textDesc) 
        this.hint.appendChild(content);
      else 
        this.hint.insertBefore(content, this.textDesc);
    }
    this.matchText = function(input) {
      console.log(input.isUpper());
      if (input.isUpper()) {
        var inArr = input.split(" ");
        var match = true;
        for (var i=0; i<inArr.length; i++) {
          if (!this.element.textContent.toUpperCase().match(inArr[i])) {
            return false;
          }
        }
        return true;
      }
      else 
        return (this.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) == 0);
    }
  }

  const __mouseEvent = function (e, ev) {
    var mouseEvent = document.createEvent("MouseEvent");
    mouseEvent.initMouseEvent(ev, true, true, e.win, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
    e.dispatchEvent(mouseEvent);
  }
  const __clickElement = function (element, ev) {
    if (ev) {
      __mouseEvent(element, ev);
    }
    else { // both events, if no event is given
      __mouseEvent(element, "click");
      __mouseEvent(element, "mousedown");
    }
  }
  const __getActive = function () {
    return _active;
  }
  const __setActive = function (element) {
    var active = __getActive();
    if (active) {
      if (_markHints) 
        active.overlay.style.background = _normalColor;
      else if (active.overlay) 
        active.overlay.parentNode.removeChild(active.overlay);
      active.hint.style.font = _font;

    }
    _active = element;
    if (!_active.overlay) {
      _active.createOverlay();
    }
    if (!_markHints)
      _active.hint.parentNode.appendChild(_active.overlay);
    _active.overlay.style.background = _activeColor;
    _active.hint.style.fontSize = _bigFont;
  }
  const __hexToRgb = function (color) {
    var rgb;
    if (color[0] !== '#')
      return color;
    if (color.length == 4) {
      rgb = /#([0-9a-f])([0-9a-f])([0-9a-f])/i.exec(color);
      for (var i=1; i<=3; i++) {
        var v = parseInt("0x" + rgb[i])+1;
        rgb[i] = v*v-1;
      }
    }
    else {
      rgb  = /#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})/i.exec(color);
      for (var i=1; i<=3; i++) {
        rgb[i] = parseInt("0x" + rgb[i]);
      }
    }
    return "rgba(" + rgb.slice(1) + "," +  _hintOpacity/2 + ")";
  }
  const __createStyleSheet = function(doc) {
    if (doc.hasStyleSheet) 
      return;
    styleSheet = __createElement("style");
    styleSheet.innerHTML += ".dwb_hint { " +
      "position:absolute; z-index:20000;" +
      "background:" + _bgColor  + ";" + 
      "color:" + _fgColor + ";" + 
      "border:" + _hintBorder + ";" + 
      "font:" + _font + ";" + 
      "display:inline;" +
      "opacity: " + _hintOpacity + "; }" + 
      ".dwb_overlay_normal { position:absolute!important;display:block!important; z-index:19999;background:" + _normalColor + ";}";
    doc.head.appendChild(styleSheet);
    doc.hasStyleSheet = true;
  }

  const __createHints = function(win, constructor, type) {
    try {
      var doc = win.document;
      _actualType = type;
      var res = doc.body.querySelectorAll(_hintTypes[type]); 
      var e, r;
      __createStyleSheet(doc);
      var hints = doc.createDocumentFragment();
      for (var i=0;i < res.length; i++) {
        e = res[i];
        if ((r = __getVisibility(e, win)) == null) 
          continue;
        if ( (e instanceof HTMLFrameElement || e instanceof HTMLIFrameElement)) {
          __createHints(e.contentWindow, constructor);
        }
        else {
          var element = new constructor(e, win, r);
          _elements.push(element);
          hints.appendChild(element.hint);
          if (_markHints) {
            element.createOverlay();
            hints.appendChild(element.overlay);
          }
        }
      }
      doc.body.appendChild(hints);
    }
    catch(exc) {
      console.error(exc);
    }
  }
  const __showHints = function (type) {
    if (document.activeElement) 
      document.activeElement.blur();

    _actualType = type;
    __createHints(window, _style == "letter" ? __letterHint : __numberHint, type);
    var l = _elements.length;

    if (l == 0) 
      return "_dwb_no_hints_";
    else if (l == 1)  {
      return  __evaluate(_elements[0].element);
    }

    for (var i=0; i<l; i++) {
      var e =_elements[i];
      e.getTextHint(i, l);
    }
    _activeArr = _elements;
    __setActive(_elements[0]);
  }
  const __updateHints = function(input) {
    var array = [];
    if (!_activeArr.length) {
      __clear();
      __showHints();
    }
    if (_lastInput && (_lastInput.length > input.length)) {
      __clear();
      _lastInput = input;
      __showHints();
      __updateHints(input);
      return;
    }
    _lastInput = input;
    if (input) {
      if (_style == "number") {
        if (input[input.length-1].isInt())
          input = input.match(/[0-9]+/g).join("");
        else 
          input = input.match(/[^0-9]+/g).join("");
      }
      else if (_style == "letter") {
        var lowerSeq = _letterSeq.toLowerCase();
        if (lowerSeq.indexOf(input.charAt(input.length-1)) == -1) 
          return "_dwb_no_hints_";
        if (input[input.length-1].isLower())
          input = input.match(new RegExp("[" + lowerSeq + "]", "g")).join("");
        else 
          input = input.match(new RegExp("[^" + lowerSeq + "]", "g")).join("");
      }
    }
    for (var i=0; i<_activeArr.length; i++) {
      var e = _activeArr[i];
      if (e.matchText(input)) {
        array.push(e);
      }
      else {
        e.hint.style.visibility = 'hidden';
      }
    }
    _activeArr = array;
    active = array[0];
    if (array.length == 0) {
      __clear();
      return "_dwb_no_hints_";
    }
    else if (array.length == 1) {
      return __evaluate(array[0].element);
    }
    else {
      _lastPosition = array[0].betterMatch(input);
      __setActive(array[_lastPosition]);
    }
  }
  const __getVisibility = function (e, win) {
    var style = win.getComputedStyle(e, null);
    if ((style.getPropertyValue("visibility") == "hidden" || style.getPropertyValue("display") == "none" ) ) {
      return null;
    }
    var r = e.getBoundingClientRect();

    var height = window.innerHeight ? window.innerHeight : document.body.offsetHeight;
    var width = window.innerWidth ? window.innerWidth : document.body.offsetWidth;
    if (!r || r.top > height || r.bottom < 0 || r.left > width ||  r.right < 0 || !e.getClientRects()[0]) {
      return null;
    }
    return r;
  }
  const __clear = function() {
    var p;
    try {
      if (_elements) {
        for (var i=0; i<_elements.length; i++) {
          var e = _elements[i];
          if (p = e.hint.parentNode) 
            p.removeChild(e.hint);
          if (e.overlay != undefined && (p = e.overlay.parentNode)) 
            p.removeChild(e.overlay);
        }
      }
      if(! _markHints && _active)
        _active.element.removeAttribute("dwb_highlight");
    }
    catch (e) { 
      console.error(e); 
    }
    _elements = [];
    _activeArr = [];
    _active = null;
    _lastPosition = 0;
  }
  const __evaluate = function (e) {
    var ret = null;
    var type;
    if (e.type) 
      type = e.type.toLowerCase();
    var tagname = e.tagName.toLowerCase();

    if (tagname && (tagname == "input" || tagname == "textarea") ) {
      if (type == "radio" || type == "checkbox") {
        e.focus();
        __clickElement(e, "click");
        ret = "_dwb_check_";
      }
      else if (type == "submit" || type == "reset" || type  == "button") {
        __clickElement(e, "click");
        ret = "_dwb_click_";
      }
      else {
        e.focus();
        ret = "_dwb_input_";
      }
    }
    else {
      if (tagname == "a" || e.hasAttribute("onclick")) 
        __clickElement(e, "click");
      else if (e.hasAttribute("onmousedown")) 
        __clickElement(e, "mousedown");
      else {
        __clickElement(e);
      }
      ret = "_dwb_click_";
    }
    __clear();
    return ret;
  }
  const __focusNext = function()  {
    var newpos = _lastPosition == _activeArr.length-1 ? 0 : _lastPosition + 1;
    active = _activeArr[newpos];
    __setActive(active);
    _lastPosition = newpos;
  }
  const __focusPrev = function()  {
    var newpos = _lastPosition == 0 ? _activeArr.length-1 : _lastPosition - 1;
    active = _activeArr[newpos];
    __setActive(active);
    _lastPosition = newpos;
  }
  const __addSearchEngine = function() {
    try {
      __createStyleSheet(document);
      var hints = document.createDocumentFragment();
      var res = document.body.querySelectorAll("form");
      var r;

      for (var i=0; i<res.length; i++) {
        var els = res[i].elements;
        for (var j=0; j<els.length; j++) {
          if (((r = __getVisibility(els[j], window)) != null) && (els[j].type == "text" || els[j].type == "search")) {
            var e = new __letterHint(els[j], window, r);
            hints.appendChild(e.hint);
            e.hint.style.visibility = "hidden";
            _elements.push(e);
          }
        }
      }
      if (_elements.length == 0) {
        return "_dwb_no_hints_";
      }
      if (_markHints) {
        for (var i=0; i<_elements.length; i++) {
          var e = _elements[i];
          e.createOverlay();
          hints.appendChild(e.overlay);
        }
      }
      else {
        var e = _elements[0];
        e.createOverlay();
        hints.appendChild(e.overlay);
      }
      document.body.appendChild(hints); 
      __setActive(_elements[0]);
      _activeArr = _elements;
    }
    catch (e) {
      console.error(e);
    }
  }
  const __submitSearchEngine = function (string) {
    var e = __getActive().element;
    e.value = string;
    e.form.submit();
    e.value = "";
    __clear();
    if (e.form.method.toLowerCase() == 'post') {
      return e.name;
    }
    return null;
  }
  const __focusInput = function() {
    var res = document.body.querySelectorAll('input[type=text], input[type=password], textarea');
    if (res.length == 0) {
      return "_dwb_no_input_";
    }
    styles = document.styleSheets[0];
    styles.insertRule('input:focus { outline: 2px solid #1793d1; }', 0);
    if (!_activeInput) {
      _activeInput = res[0];
    }
    else {
      for (var i=0; i<res.length; i++) {
        if (res[i] == _activeInput) {
          if (!(_activeInput = res[i+1])) {
            _activeInput = res[0];
          }
          break;
        }
      }
    }
    _activeInput.focus();
  }
  const __init = function (letter_seq, font, style,
          fg_color, bg_color, active_color, normal_color, border,  opacity, markHints) {
        _letterSeq  = letter_seq;
        _font = font;
        _style =  style.toLowerCase();
        _fgColor    = fg_color;
        _bgColor    = bg_color;
        _activeColor = __hexToRgb(active_color);
        _normalColor = __hexToRgb(normal_color);
        _hintBorder = border;
        _hintOpacity = opacity;
        _markHints = markHints;
        _bigFont = Math.ceil(font.replace(/\D/g, "") * 1.25) + "px";
  }


  return {
    createStylesheet : function() {
      __createStyleSheet(document);
    },
    showHints : 
      function(type) {
        return __showHints(type);
      },
    updateHints :
      function (input) {
        return __updateHints(input);
      },
    clear : 
      function () {
        __clear();
      },
    followActive :
      function () {
        return __evaluate(__getActive().element);
      },

    focusNext :
      function () {
        __focusNext();
      },
    focusPrev : 
      function () {
        __focusPrev();
      },
    addSearchEngine : 
      function () {
        return __addSearchEngine();
      },
    submitSearchEngine :
      function (string) {
        return __submitSearchEngine(string);
      },
    focusInput : 
      function () {
        __focusInput();
      },
    init: 
      function (letter_seq, font, style,
          fg_color, bg_color, active_color, normal_color, border,  opacity, highlightLinks) {
        __init(letter_seq, font, style, fg_color, bg_color, active_color, normal_color, border,  opacity, highlightLinks); 
      }, 
  }
})();
