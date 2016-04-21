var content = "Content-type";
var cache = "cache-control"
var priv = "private";
//var ctype = "application/text";
var ctype = "application/x-www-form-urlencoded";
var cjson = "application/json";
function reloadAfter($seconds) {
	setTimeout(function(){
	   window.location.replace("/");
	}, $seconds*1000);
}
function onRangeChange($range, $spanid, $mul, $rotate) {
	var val = document.getElementById($range).value;
	if($rotate) val = document.getElementById($range).max - val;
	document.getElementById($spanid).innerHTML = (val * $mul) + " dB";
	saveSoundSettings();
}
function onRangeChangeFreqTreble($range, $spanid, $mul, $rotate) {
	var val = document.getElementById($range).value;
	if($rotate) val = document.getElementById($range).max - val;
	document.getElementById($spanid).innerHTML = "From "+(val * $mul) + " kHz";
	saveSoundSettings();
}
function onRangeChangeFreqBass($range, $spanid, $mul, $rotate) {
	var val = document.getElementById($range).value;
	if($rotate) val = document.getElementById($range).max - val;
	document.getElementById($spanid).innerHTML = "Under "+(val * $mul) + " Hz";
	saveSoundSettings();
}
function onRangeChangeSpatial($range, $spanid) {
	var val = document.getElementById($range).value;
	var label;
	switch (val){
		case '0': label="Off";break;
		case '1': label="Minimal";break;
		case '2': label="Normal";break;
		case '3': label="Maximal";break;		
	}
	document.getElementById($spanid).innerHTML = label;
	saveSoundSettings();
}
function onRangeVolChange($value) {

	var val = document.getElementById('vol_range').max - $value;
	document.getElementById('vol1_span').innerHTML = (val * -0.5) + " dB";
	document.getElementById('vol_span').innerHTML = (val * -0.5) + " dB";
	document.getElementById('vol_range').value = $value;
	document.getElementById('vol1_range').value = $value;
	xhr = new XMLHttpRequest();
	xhr.open("POST","soundvol",false);
	xhr.setRequestHeader(content,ctype);
	xhr.send(  "vol=" + document.getElementById('vol_range').value+"&");
}
function instantPlay() {
	xhr = new XMLHttpRequest();
	xhr.open("POST","instant_play",false);
	xhr.setRequestHeader(content,ctype);
	xhr.setRequestHeader(cache, priv);
	if (!(document.getElementById('instant_path').value.substring(0, 1) === "/")) document.getElementById('instant_path').value = "/" + document.getElementById('instant_path').value;
	document.getElementById('instant_url').value = document.getElementById('instant_url').value.replace(/^https?:\/\//,'');
	xhr.send("url=" + document.getElementById('instant_url').value + "&port=" + document.getElementById('instant_port').value + "&path=" + document.getElementById('instant_path').value+"&");
	window.location.replace("/");
}
function playStation() {
	select = document.getElementById('stationsSelect');
	localStorage.setItem('selindexstore', select.options.selectedIndex.toString());
	xhr = new XMLHttpRequest();
	xhr.open("POST","play",false);
	xhr.setRequestHeader(content,ctype);
	xhr.setRequestHeader(cache, priv);
	xhr.send("id=" + select.options[select.options.selectedIndex].id+"&");
	window.location.replace("/");
//    window.location.reload(false);
}
function stopStation() {
	var select = document.getElementById('stationsSelect');
	localStorage.setItem('selindexstore', select.options.selectedIndex.toString());
	xhr = new XMLHttpRequest();
	xhr.open("POST","stop",false);
	xhr.setRequestHeader(content,ctype);
	xhr.setRequestHeader(cache, priv);
	xhr.send("id=" + select.options[select.options.selectedIndex].id+"&");
	window.location.replace("/");
}
function saveSoundSettings() {
	xhr = new XMLHttpRequest();
	xhr.open("POST","sound",false);
	xhr.setRequestHeader(content,ctype);
	xhr.setRequestHeader(cache, priv);
	xhr.send(
	           "&bass=" + document.getElementById('bass_range').value 
			 +"&treble=" + document.getElementById('treble_range').value
	         + "&bassfreq=" + document.getElementById('bassfreq_range').value 
			 + "&treblefreq=" + document.getElementById('treblefreq_range').value
			 + "&spacial=" + document.getElementById('spacial_range').value
			 + "&");
}
function saveStation() {
	var file = document.getElementById('add_path').value;
	var url = document.getElementById('add_url').value;
	if (!(file.substring(0, 1) === "/")) file = "/" + file;
	url = url.replace(/^https?:\/\//,'');
	xhr = new XMLHttpRequest();
	xhr.open("POST","setStation",false);
	xhr.setRequestHeader(content,ctype);
	xhr.setRequestHeader(cache, priv);
	xhr.send("id=" + document.getElementById('add_slot').value + "&url=" + url + "&name=" + document.getElementById('add_name').value + "&file=" + file + "&port=" + document.getElementById('add_port').value+"&");
	sessionStorage.setItem(document.getElementById('add_slot').value,"{\"Name\":\""+document.getElementById('add_name').value+"\",\"URL\":\""+url+"\",\"File\":\""+file+"\",\"Port\":\""+document.getElementById('add_port').value+"\"}");
//	sessionStorage.clear();
//	window.location.replace("/");
	window.location.reload(false);
}
function editStation(id) {
	function cpedit() {
			document.getElementById('add_url').value = arr["URL"];
			document.getElementById('add_name').value = arr["Name"];
			document.getElementById('add_path').value = arr["File"];
			document.getElementById('add_port').value = arr["Port"];
			document.getElementById('editStationDiv').style.display = "block";
			setMainHeight("tab-content2");
	}
	document.getElementById('add_slot').value = id;
	idstr = id.toString();			
	if (sessionStorage.getItem(idstr) != null)
	{	
		var arr = JSON.parse(sessionStorage.getItem(idstr));
		cpedit();
	}
	else {
	xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function() {
		if (xhr.readyState == 4 && xhr.status == 200) {
			var arr = JSON.parse(xhr.responseText);
			cpedit();
		}
	}
	xhr.open("POST","getStation",false);
	xhr.setRequestHeader(content,ctype);
	xhr.setRequestHeader(cache, priv);
	xhr.send("idgp=" + id+"&");
	}
//	sessionStorage.clear();
}

function loadStations(page) {
	var new_tbody = document.createElement('tbody');
	var id = 16 * (page-1);
	function cploadStations(id,arr) {
					var tr = document.createElement('TR');
					var td = document.createElement('TD');
					td.appendChild(document.createTextNode(id + 1));
					td.style.width = "10%";
					tr.appendChild(td);
					for(var key in arr){
						var td = document.createElement('TD');
						if(arr[key].length > 64) arr[key] = "Error";
						td.appendChild(document.createTextNode(arr[key]));
						tr.appendChild(td);
					}
					var td = document.createElement('TD');
					td.innerHTML = "<a href=\"#\" onClick=\"editStation("+id+")\">Edit</a>";
					tr.appendChild(td);
					new_tbody.appendChild(tr);
}	
	for(id; id < 16*page; id++) {
		idstr = id.toString();		
		if (sessionStorage.getItem(idstr) != null)
		{	
			var arr = JSON.parse(sessionStorage.getItem(idstr));
			cploadStations(id,arr);
		}
		else
		{
			xhr = new XMLHttpRequest();
			xhr.onreadystatechange = function() {
				if (xhr.readyState == 4 && xhr.status == 200) {
					var arr = JSON.parse(xhr.responseText);
					sessionStorage.setItem(idstr,xhr.responseText);
					cploadStations(id,arr);
				}
			}
			xhr.open("POST","getStation",false);
			xhr.setRequestHeader(content,ctype);
			xhr.send("idgp=" + id+"&");
		}
	}
//	console.log(new_tbody);
	var old_tbody = document.getElementById("stationsTable").getElementsByTagName('tbody')[0];
	old_tbody.parentNode.replaceChild(new_tbody, old_tbody);
	setMainHeight("tab-content2");
}

function loadStationsList(max) {
	var foundNull = false;
	function cploadStationsList(id,arr) {
		var foundNull = false;
			if(arr["Name"].length > 0) 
			{
				var opt = document.createElement('option');
				opt.appendChild(document.createTextNode(arr["Name"]));
				opt.id = id;
				document.getElementById("stationsSelect").appendChild(opt);
			} else foundNull = true;
			return foundNull;
	}		
	document.getElementById("stationsSelect").disabled = true;
	for(var id=0; id<max; id++) {
		if (foundNull) break;
		idstr = id.toString();
		if (sessionStorage.getItem(idstr) != null)
		{	
			var arr = JSON.parse(sessionStorage.getItem(idstr));
			foundNull = cploadStationsList(id,arr);
		}
		else
		{
			xhr = new XMLHttpRequest();
			xhr.onreadystatechange = function() {			
				if (xhr.readyState == 4 && xhr.status == 200) {
					var arr = JSON.parse(xhr.responseText);
					sessionStorage.setItem(idstr,xhr.responseText);
					foundNull = cploadStationsList(id,arr);
				}
			}
			xhr.open("POST","getStation",false);
			xhr.setRequestHeader(content,ctype);
			xhr.send("idgp=" + id+"&");
		}
	}
	document.getElementById("stationsSelect").disabled = false;
	select = document.getElementById('stationsSelect');
	select.options.selectedIndex= parseInt(localStorage.getItem('selindexstore'));
//	getSelIndex();
}
/*	
function getSelIndex() {
		xhr = new XMLHttpRequest();
		xhr.onreadystatechange = function() {
			if (xhr.readyState == 4 && xhr.status == 200) {
				console.log("JSON: " + xhr.responseText);
				var arr = JSON.parse(xhr.responseText);
				if(arr["Index"].length > 0) {
					document.getElementById("stationsSelect").options.selectedIndex = arr["Index"];
					document.getElementById("stationsSelect").disabled = false;
					console.log("selIndex received " + arr["Index"]);
				} 
			}
		}
		xhr.open("POST","getSelIndex",true);
		xhr.setRequestHeader(content,ctype);
		xhr.send();	
}	*/
function setMainHeight(name) {
	var minh = window.innerHeight;
	var h = document.getElementById(name).offsetHeight + 200;
	if(h<minh) h = minh;
	document.getElementById("MAIN").style.height = h;
}
document.addEventListener("DOMContentLoaded", function() {
	document.getElementById("tab1").addEventListener("click", function() {
			setMainHeight("tab-content1");
		});
	document.getElementById("tab2").addEventListener("click", function() {
			loadStations(1);
			setMainHeight("tab-content2");
		});
	document.getElementById("tab3").addEventListener("click", function() {
			setMainHeight("tab-content3");
		});

	onRangeChange('treble_range', 'treble_span', 1.5, false);
	onRangeChange('bass_range', 'bass_span', 1, false);
	onRangeChangeFreqTreble('treblefreq_range', 'treblefreq_span', 1, false);
	onRangeChangeFreqBass('bassfreq_range', 'bassfreq_span', 10, false);
	onRangeChangeSpatial('spacial_range', 'spacial_span');
	onRangeChange('vol_range', 'vol1_span', -0.5, true);
	onRangeChange('vol_range', 'vol_span', -0.5, true);
	loadStationsList(192);
	onRangeChange('vol1_range', 'vol1_span', -0.5, true);
	setMainHeight("tab-content1");
});
