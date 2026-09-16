(function(){
  if (document.cookie.indexOf('avaui_vw=') !== -1) return;
  var step = 24;
  var w = Math.round(window.innerWidth / step) * step;
  var h = Math.round(window.innerHeight / step) * step;
  document.cookie = 'avaui_vw=' + w + '; path=/; max-age=86400; SameSite=Lax';
  document.cookie = 'avaui_vh=' + h + '; path=/; max-age=86400; SameSite=Lax';
  location.reload();
})();
