(function(){
  function findHandler(el, domEventName){
    while (el && el !== document.body) {
      for (var n = 1; ; n++) {
        var suffix = n === 1 ? '' : ('-' + n);
        var evAttr = el.getAttribute('data-event' + suffix);
        if (!evAttr) break;
        if (evAttr === domEventName) {
          var handler = el.getAttribute('data-handler' + suffix);
          if (handler) return handler;
        }
      }
      el = el.parentElement;
    }
    return null;
  }
  function applyHtml(html){
    var next = new DOMParser().parseFromString(html, 'text/html');
    if (next.title) document.title = next.title;
    var nextViewport = next.getElementById('ava-viewport');
    var viewport = document.getElementById('ava-viewport');
    var scrollRoot = (nextViewport && viewport) ? viewport : document.body;
    var oldScrollviews = scrollRoot.querySelectorAll('.ava-scrollview');
    var savedScroll = [];
    oldScrollviews.forEach(function(el){
      savedScroll.push({top: el.scrollTop, left: el.scrollLeft});
    });
    var closingClones = [];
    if (nextViewport && viewport) {
      var newIds = {};
      nextViewport.querySelectorAll('.ava-overlay-fragment[data-dialog-id]').forEach(function(el){
        newIds[el.getAttribute('data-dialog-id')] = true;
      });
      viewport.querySelectorAll('.ava-overlay-fragment[data-dialog-id]').forEach(function(el){
        if (!newIds[el.getAttribute('data-dialog-id')]) closingClones.push(el.cloneNode(true));
      });
    }
    if (nextViewport && viewport) {
      viewport.innerHTML = nextViewport.innerHTML;
    } else {
      document.body.innerHTML = next.body.innerHTML;
    }
    var newScrollRoot = (nextViewport && viewport) ? viewport : document.body;
    var newScrollviews = newScrollRoot.querySelectorAll('.ava-scrollview');
    newScrollviews.forEach(function(el, i){
      var pos = savedScroll[i];
      if (!pos) return;
      el.scrollTop = pos.top;
      el.scrollLeft = pos.left;
    });
    closingClones.forEach(function(clone){
      clone.classList.add('ava-dialog-closing');
      newScrollRoot.appendChild(clone);
      var done = false;
      var finish = function(){
        if (done) return;
        done = true;
        if (clone.parentNode) clone.parentNode.removeChild(clone);
      };
      clone.addEventListener('animationend', finish);
      setTimeout(finish, 400);
    });
    return newScrollRoot;
  }
  function fireHandler(handler, preventDefault, ev, sourceEl){
    if (preventDefault && ev) ev.preventDefault();
    var body = 'handler=' + encodeURIComponent(handler);
    var compId = sourceEl ? sourceEl.getAttribute('data-comp-id') : null;
    var focusState = null;
    if (compId) {
      body += '&compId=' + encodeURIComponent(compId) +
              '&value=' + encodeURIComponent(sourceEl.value);
      if (document.activeElement === sourceEl) {
        focusState = {
          compId: compId,
          start: (typeof sourceEl.selectionStart === 'number') ? sourceEl.selectionStart : null,
          end: (typeof sourceEl.selectionEnd === 'number') ? sourceEl.selectionEnd : null
        };
      }
    }
    fetch('/__avahost/event?path=' + encodeURIComponent(location.pathname), {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body
    }).then(function(r){ return r.text(); }).then(function(html){
      var newScrollRoot = applyHtml(html);
      if (focusState) {
        var restored = newScrollRoot.querySelector('[data-comp-id="' + focusState.compId + '"]');
        if (restored) {
          restored.focus();
          if (focusState.start !== null && typeof restored.setSelectionRange === 'function') {
            try { restored.setSelectionRange(focusState.start, focusState.end); } catch (e) {}
          }
        }
      }
    }).catch(function(){});
  }
  var eventMap = {
    'click': 'click',
    'onchange': 'change',
    'oninput': 'input',
    'onfocus': 'focusin',
    'onblur': 'focusout',
    'onkeydown': 'keydown',
    'onkeyup': 'keyup',
    'onmouseenter': 'mouseover',
    'onmouseleave': 'mouseout',
    'onsubmit': 'submit'
  };
  Object.keys(eventMap).forEach(function(avauiName){
    var domEventName = eventMap[avauiName];
    document.body.addEventListener(domEventName, function(ev){
      var el = ev.target.closest('[data-handler],[data-handler-2],[data-handler-3],[data-handler-4],[data-handler-5]');
      if (!el) return;
      var handler = findHandler(el, avauiName);
      if (!handler) return;
      fireHandler(handler, avauiName === 'click' || avauiName === 'onsubmit', ev, el);
    }, avauiName === 'onfocus' || avauiName === 'onblur');
  });
  document.addEventListener('load', function(ev){
    var el = ev.target && ev.target.closest ? ev.target.closest('[data-handler],[data-handler-2],[data-handler-3],[data-handler-4],[data-handler-5]') : null;
    if (!el) return;
    var handler = findHandler(el, 'onload');
    if (!handler) return;
    fireHandler(handler, false, ev);
  }, true);
  document.addEventListener('error', function(ev){
    var el = ev.target && ev.target.closest ? ev.target.closest('[data-handler],[data-handler-2],[data-handler-3],[data-handler-4],[data-handler-5]') : null;
    if (!el) return;
    var handler = findHandler(el, 'onerror');
    if (!handler) return;
    fireHandler(handler, false, ev);
  }, true);

  function readViewportCookie(name, fallback){
    var m = document.cookie.match(new RegExp('(?:^|; )' + name + '=([0-9]+)'));
    return m ? parseInt(m[1], 10) : fallback;
  }
  var resizeStep = 24;
  var lastW = readViewportCookie('avaui_vw', 1280);
  var lastH = readViewportCookie('avaui_vh', 720);
  var resizeTimer = null;
  function applyResize(){
    var w = Math.round(window.innerWidth / resizeStep) * resizeStep;
    var h = Math.round(window.innerHeight / resizeStep) * resizeStep;
    if (w === lastW && h === lastH) return;
    document.cookie = 'avaui_vw=' + w + '; path=/; max-age=86400; SameSite=Lax';
    document.cookie = 'avaui_vh=' + h + '; path=/; max-age=86400; SameSite=Lax';
    fetch(location.pathname + location.search).then(function(r){ return r.text(); }).then(function(html){
      lastW = w; lastH = h;
      applyHtml(html);
    }).catch(function(){ location.reload(); });
  }
  window.addEventListener('resize', function(){
    if (resizeTimer) clearTimeout(resizeTimer);
    resizeTimer = setTimeout(applyResize, 200);
  });
})();
