BEGIN { 
	if(!title) title = "Documentation"
	print "<!DOCTYPE html>\n<html>\n<head>\n<title>" title "</title>";
	print "<style><!--";
	print "body {font-family:Arial, Verdana, Helvetica, sans-serif;margin-left:20px;margin-right:20px;}";
	print "h1 {color:#575c91;border:none;padding:5px;}";
	print "h2 {color:#575c91;border:none;padding:5px;}";
	print "h3 {color:#9191c1;border:none;padding:5px;}";
	print "a{padding:2px;border-radius:2px;}";
	print "a:link {color: #575c91;}";
	print "a:visited {color: #575c91;text-decoration:none;}";
	print "a:active {background:#575c91;color:#f0f0ff;}";
	print "a:hover {background:#b8b8e0;color:#f0f0ff;}";
	print "code,strong {color:#575c91}";
	print "pre {color:#575c91;background:#d4d4ff;border:none;border-radius:5px;padding:7px;margin-left:15px;margin-right:15px;}";
	print "div.title {color:#575c91;font-weight:bold;background:#b8b8e0;border:none;border-radius:5px;padding:10px;margin:10px 5px;font-family:monospace;}";
	print "div.box {background:#f0f0ff;border:none;border-radius:5px;margin:10px 2px;padding:1px;}";
	print "div.inner-box {border:none;margin:5px;padding:3px;}";
	print "--></style>";
	print "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">";
	print "</head>\n<body>";
}

/\/\*/ { comment = 1; }

/\*1/ { if(!comment) next; s = substr($0, index($0, "*1") + 2); print "<h1>" filter(s) "</h1>"; next;}
/\*2/ { if(!comment) next; s = substr($0, index($0, "*2") + 2); print "<h2>" filter(s) "</h2>"; next;}
/\*3/ { if(!comment) next; s = substr($0, index($0, "*3") + 2); print "<h3>" filter(s) "</h3>"; next;}
/\*@/ { if(!comment) next; s = substr($0, index($0, "*@") + 2); print "<div class=\"box\"><div class=\"title\">" filter(s) "</div><div class=\"inner-box\">"; div+=2; next;}
/\*#[ \t\r]*$/ { if(!comment) next; if(!pre) print "<br>"; next;}
/\*#/ { if(!comment) next; s = substr($0, index($0, "*#") + 2); print filter(s);}
/\*&/ { if(!comment) next; s = substr($0, index($0, "*&") + 2); print "<code>" filter(s) "</code><br>"; next;}
/\*X/ { if(!comment) next; s = substr($0, index($0, "*X") + 2); print "<p><strong>Example:</strong><code>" filter(s) "</code></p>"; next;}
/\*N/ { if(!comment) next; s = substr($0, index($0, "*N") + 2); print "<p><strong>Note:</strong><em>" filter(s) "</em></p>"; next;}
/\*\[/ { if(!comment) next; pre=1; print "<pre>"; next;}
/\*]/ { if(!comment) next; pre=0; print "</pre>"; next;}
/\*\{/ { if(!comment) next; print "<ul>"; next;}
/\*\*/ { if(!comment) next; s = substr($0, index($0, "**") + 2); print "<li>" filter(s); next;}

# Mistake on my part where *} at the begining of a line clashes with the *} to insert the </strong>
/^[ \t]*\*}/ { if(!comment) next; print "</ul>"; next;}

/\*-/ { if(!comment) next; print "<hr size=2>"; next;}
/\*=/ { if(!comment) next; print "<hr size=5>"; next;}

/\*\// { comment=0; while(div > 0) {print "</div>"; div--;} }

END { print "</body></html>" }

function filter(ss,        j, k1, k2, k3)
{
	gsub(/&/, "\\&amp;", ss); 
	gsub(/</, "\\&lt;", ss); 
	gsub(/>/, "\\&gt;", ss);
	gsub(/\\n[ \t\r]*$/, "<br>", ss);
	gsub(/{{/, "<code>", ss); 
	gsub(/}}/, "</code>", ss);
	gsub(/{\*/, "<strong>", ss); 
	gsub(/\*}/, "</strong>", ss);
	gsub(/{\//, "<em>", ss); 
	gsub(/\/}/, "</em>", ss);
	gsub(/{_/, "<u>", ss); 
	gsub(/_}/, "</u>", ss);	
	
	# Hyperlinks (excuse my primitive regex)
	gsub(/http:\/\/[a-zA-Z0-9._\/\-%~]+/, "<a href=\"&\">&</a>", ss);
	
	# Use a ##word to specify an anchor, eg. ##foo gets translated to <span id="foo">foo</a>
	while(j = match(ss, /##[A-Za-z0-9_]+/)) {
		k1 = substr(ss, 1, j - 1);
		k2 = substr(ss, j + 2, RLENGTH-2);
		k3 = substr(ss, j + RLENGTH);
		ss = k1 "<span id=\"" k2 "\">" k2 "</span>" k3
	}
	
	# Use a ~~word to specify an anchor, eg. ~~foo gets translated to <a href="#foo">foo</a>
	while(j = match(ss, /~~[A-Za-z0-9_]+/)) {
		k1 = substr(ss, 1, j - 1);
		k2 = substr(ss, j + 2, RLENGTH-2);
		k3 = substr(ss, j + RLENGTH);
		ss = k1 "<a href=\"#" k2 "\">" k2 "</a>" k3
	}
	
	gsub(/\*\//, "", ss);
	return ss;
}
