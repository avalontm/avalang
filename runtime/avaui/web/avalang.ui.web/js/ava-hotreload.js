(function(){
  var v=null;
  setInterval(function(){
    fetch('/__avahost/hotreload').then(function(r){return r.text();}).then(function(t){
      if (v===null) { v=t; return; }
      if (t!==v) { location.reload(); }
    }).catch(function(){});
  }, 1000);
})();
