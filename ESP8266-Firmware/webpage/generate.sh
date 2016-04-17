#!/bin/bash
python3 /usr/local/bin/css-html-js-minify.py --checkupdates style.css
mv style.css style.ori
mv style.min.css style.css
xxd -i style.css > style
sed -i 's/\[\]/\[\] ICACHE_STORE_ATTR ICACHE_RODATA_ATTR /g' style
mv style.ori style.css

#python3 /usr/local/bin/css-html-js-minify.py script.js
#mv script.js script.ori
#mv script.min.js script.js
xxd -i script.js > script
sed -i 's/\[\]/\[\] ICACHE_STORE_ATTR ICACHE_RODATA_ATTR /g' script
#mv script.ori script.js

#mv index.html index.htm
#python3 /usr/local/bin/css-html-js-minify.py index.htm
xxd -i index.html > index
sed -i 's/\[\]/\[\] ICACHE_STORE_ATTR ICACHE_RODATA_ATTR /g' index
#rm index.html
#mv index.htm index.html
