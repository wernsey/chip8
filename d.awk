#! /usr/bin/awk -f

##
# d.awk
# =====
#
# Converts Markdown in C/C++-style code comments to HTML.  \
# <https://github.com/wernsey/d.awk>
#
# The comments must have the `/** */` pattern. Every line in the comment
# must start with a *. Like so:
#
# ```c
# /**
#  * Markdown here...
#  */
# ```
#
# Alternatively, three slashes can also be used: `/// Markdown here`
#
# ## Configuration Options
#
# You can set these in the BEGIN block below, or pass them to the script through the
# `-v` command-line option:
#
# - `-vTitle="My Document Title"` to set the `<title/>` in the `<head/>` section of the HTML
# - `-vStyleSheet=style.css` to use a separate CSS file as style sheet.
# - `-vTheme=n` with n either 0 to disable CSS or 1 to enable; Default = 1
# - `-vTopLinks=1` to have links to the top of the doc next to headers.
# - `-vMaxWidth=1080px` specifies the Maximum width of the HTML output.
# - `-vPretty=1` enable Syntax highlighting with Google's code prettify
#        https://github.com/google/code-prettify
# - `-vHideToCLevel=n` specifies the level of the ToC that should be collapsed by default.
# - `-vLang=n` specifies the value of the `lang` attribute of the <html> tag; Default = "en"
# - `-vclassic_underscore=1` words_with_underscores behave like old markdown where the
#        underscores in the word counts as emphasis. The default behaviour is to have
#        `words_like_this` not contain any emphasis.
# - `-vNumberHeadings=1` to enable or disable section numbers in front of headings; Default = 1
# - `-vNumberH1s=1`: if `NumberHeadings` is enabled, `<H1>` headings are not numbered by
#        default (because the `<H1>` would typically contain the document title). Use this to
#        number `<H1>`s as well.
#
# I've tested it with Gawk, Nawk and Mawk.
# Gawk and Nawk worked without issues, but don't use the `-W compat`
# or `-W traditional` settings with Gawk.
# Mawk v1.3.4 worked correctly but v1.3.3 choked on it.
#
# ## Extensions
#
# - Insert a Table of Contents by using `\![toc]`.
#   The Table of Contents is collapsed by default:
#   - Use `\\![toc+]` to insert a ToC that is expanded by default;
#   - Use `\\![toc-]` for a collapsed ToC.
# - Github-style ```` ``` ```` code blocks supported.
# - Github-style `~~strikethrough~~` supported.
# - GitHub-style task lists `- [x]` are supported for documenting bugs and todo lists in code.
# - The `id` attribute of anchor tags `<a>` are treated as in GitHub:
#   The tag's id should be the title, in lower case stripped of non-alphanumeric characters
#   (except hyphens and spaces) and then with all spaces replaced with hyphens.
#   then add -1, -2, -3 until it's unique
#   See [here](https://gist.github.com/asabaylus/3071099) (especially the comment by TomOnTime)
#   and [here](https://gist.github.com/rachelhyman/b1f109155c9dafffe618)
# - A couple of ideas from MultiMarkdown:
#    - `\\[^footnotes]` are supported.
#    - `*[abbr]:` Abbreviations are supported.
#    - Space followed by \\ at the end of a line also forces a line break.
# - Default behaviour is to have words_like_this not contain emphasis.
#
# Limitations:
#
# - You can't nest `<blockquote>`s, and they can't contain nested lists
#     or `pre` blocks. You can work around this by using HTML directly.
# - It takes some liberties with how inline (particularly block-level) HTML is processed and not
#     all HTML tags supported. HTML forms and `<script/>` tags are out.
# - Paragraphs in lists differ a bit from other markdowns. Use indented blank lines to get
#    to insert `<br>` tags to emulate paragraphs. Blank lines stop the list by inserting the
#    `</ul>` or `</ol>` tags.
#
# ## References
#
# - <https://tools.ietf.org/html/rfc7764>
# - <http://daringfireball.net/projects/markdown/syntax>
# - <https://guides.github.com/features/mastering-markdown/>
# - <http://fletcher.github.io/MultiMarkdown-4/syntax>
# - <http://spec.commonmark.org>
#
# ## License
#
#     (c) 2016 Werner Stoop
#     Copying and distribution of this file, with or without modification,
#     are permitted in any medium without royalty provided the copyright
#     notice and this notice are preserved. This file is offered as-is,
#     without any warranty.

BEGIN {

    # Configuration options
    if(Title== "") Title = "Documentation";
    if(Theme== "") Theme = 1;
    if(Pretty== "") Pretty = 0;
    if(HideToCLevel== "") HideToCLevel = 3;
    if(Lang == "") Lang = "en";
    #TopLinks = 1;
    #classic_underscore = 1;
    if(MaxWidth=="") MaxWidth="1080px";
    if(NumberHeadings=="") NumberHeadings = 1;
    if(NumberH1s=="") NumberH1s = 0;

    Mode = (Clean)?"p":"none";
    ToC = ""; ToCLevel = 1;
    CSS = init_css(Theme);
    for(i = 0; i < 128; i++)
        _ord[sprintf("%c", i)] = i;
    srand();
}

!Clean && !Multi && /\/\*\*/    {
    Mode = "p";
    sub(/^.*\/\*\*/,"");
    if(match($0,/\*\//)) {
        sub(/\*\/.*/,"");
        Out = Out filter($0);
        Out = Out tag(Mode, scrub(Buf));
        Buf = "";
        Prev = "";
    } else {
        Out = Out filter($0);
        Multi = 1;
    }
}

Multi && /\*\// {
    gsub(/\*\/.*$/,"");
    if(match($0, /^[[:space:]]*\*/))
        Out = Out filter(substr($0, RSTART+RLENGTH));
    if(Mode == "ul" || Mode == "ol") {
        while(ListLevel > 1)
            Buf = Buf "\n</" Open[ListLevel--] ">";
        Out = Out tag(Mode, Buf "\n");
    } else {
        Buf = trim(scrub(Buf));
        if(Buf)
            Out = Out tag(Mode, Buf);
    }
    Mode = "none";
    Multi = 0;
    Buf = "";
    Prev = "";
}
Multi {
    gsub(/\r/, "", $0);
    if(match($0,/[[:graph:]]/) && substr($0,RSTART,1)!="*")
        next;
    gsub(/^[[:space:]]*\*/, "", $0);
}
Multi { Out = Out filter($0); }

# These are the rules for `///` single-line comments:
Single && $0 !~ /\/\/\// {
    if(Mode == "ul" || Mode == "ol") {
        while(ListLevel > 1)
            Buf = Buf "\n</" Open[ListLevel--] ">";
        Out = Out tag(Mode, Buf "\n");
    } else {
        Buf = trim(scrub(Buf));
        if(Buf)
            Out = Out tag(Mode, Buf);
    }
    Mode = "none";
    Single = 0;
    Buf = "";
    Prev = "";
}
Single && /\/\/\// {
    sub(/.*\/\/\//,"");
    Out = Out filter($0);
}
!Clean && !Single && !Multi && /\/\/\// {
    sub(/.*\/\/\//,"");
    Single = 1;
    Mode = "p";
    Out = Out filter($0);
}

Clean {
    Out = Out filter($0);
}

END {

    if(Mode == "ul" || Mode == "ol") {
        while(ListLevel > 1)
            Buf = Buf "\n</" Open[ListLevel--] ">";
        Out = Out tag(Mode, Buf "\n");
    } else if(Mode == "pre") {
        while(ListLevel > 1)
            Buf = Buf "\n</" Open[ListLevel--] ">";
        Out = Out tag(Mode, Buf "\n");
    } else {
        Buf = trim(scrub(Buf));
        if(Buf)
            Out = Out tag(Mode, Buf);
    }

    print "<!DOCTYPE html>\n<html lang=\"" Lang "\"><head>"
    print "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">";
    print "<title>" Title "</title>";
    if(StyleSheet)
        print "<link rel=\"stylesheet\" href=\"" StyleSheet "\">";
    else
        print "<style><!--" CSS "\n--></style>";
    if(ToC && match(Out, /!\[toc[-+]?\]/))
        print "<script><!--\n" \
            "function toggle_toc(n) {\n" \
            "    var toc=document.getElementById('table-of-contents-' + n);\n" \
            "    var btn=document.getElementById('btn-text-' + n);\n" \
            "    toc.style.display=(toc.style.display=='none')?'block':'none';\n" \
            "    btn.innerHTML=(toc.style.display=='none')?'&#x25BA;':'&#x25BC;';\n" \
            "}\n" \
            "function toggle_toc_ul(n) {   \n" \
            "    var toc=document.getElementById('toc-ul-' + n);   \n" \
            "    var btn=document.getElementById('toc-btn-' + n);   \n" \
            "    if(toc) {\n" \
            "        toc.style.display=(toc.style.display=='none')?'block':'none';   \n" \
            "        btn.innerHTML=(toc.style.display=='none')?'&#x25BA;':'&#x25BC;';\n" \
            "    }\n" \
            "}\n" \
            "//-->\n</script>";
    if(Pretty && HasCode)
        print "<script src=\"https://cdn.rawgit.com/google/code-prettify/master/loader/run_prettify.js\"></script>";
    print "</head><body>";
    if(Out) {
        Out = fix_footnotes(Out);
        Out = fix_links(Out);
        Out = fix_abbrs(Out);
        Out = make_toc(Out);

        print trim(Out);
        if(footnotes) {
            footnotes = fix_links(footnotes);
            print "<hr><ol class=\"footnotes\">\n" footnotes "</ol>";
        }
    }
    print "</body></html>"
}

function escape(st) {
    gsub(/&/, "\\&amp;", st);
    gsub(/</, "\\&lt;", st);
    gsub(/>/, "\\&gt;", st);
    return st;
}
function strip_tags(st) {
    gsub(/<\/?[^>]+>/,"",st);
    return st;
}
function trim(st) {
    sub(/^[[:space:]]+/, "", st);
    sub(/[[:space:]]+$/, "", st);
    return st;
}
function filter(st,       res,tmp, linkdesc, url, delim, edelim, name, def, plang) {
    if(Mode == "p") {
        if(match(st, /^[[:space:]]*\[[-._[:alnum:][:space:]]+\]:/)) {
            linkdesc = ""; LastLink = 0;
            match(st,/\[.*\]/);
            LinkRef = tolower(substr(st, RSTART+1, RLENGTH-2));
            st = substr(st, RSTART+RLENGTH+2);
            match(st, /[^[:space:]]+/);
            url = substr(st, RSTART, RLENGTH);
            st = substr(st, RSTART+RLENGTH+1);
            if(match(url, /^<.*>/))
                url = substr(url, RSTART+1, RLENGTH-2);
            if(match(st, /["'(]/)) {
                delim = substr(st, RSTART, 1);
                edelim = (delim == "(") ? ")" : delim;
                if(match(st, delim ".*" edelim))
                    linkdesc = substr(st, RSTART+1, RLENGTH-2);
            }
            LinkUrls[LinkRef] = escape(url);
            if(!linkdesc) LastLink = 1;
            LinkDescs[LinkRef] = escape(linkdesc);
            return;
        } else if(LastLink && match(st, /^[[:space:]]*["'(]/)) {
            match(st, /["'(]/);
            delim = substr(st, RSTART, 1);
            edelim = (delim == "(") ? ")" : delim;
            st = substr(st, RSTART);
            if(match(st, delim ".*" edelim))
                LinkDescs[LinkRef] = escape(substr(st,RSTART+1,RLENGTH-2));
            LastLink = 0;
            return;
        } else if(match(st, /^[[:space:]]*\[\^[-._[:alnum:][:space:]]+\]:[[:space:]]*/)) {
            match(st, /\[\^[[:alnum:]]+\]:/);
            name = substr(st, RSTART+2,RLENGTH-4);
            def = substr(st, RSTART+RLENGTH+1);
            Footnote[tolower(name)] = scrub(def);
            return;
        } else if(match(st, /^[[:space:]]*\*\[[[:alnum:]]+\]:[[:space:]]*/)) {
            match(st, /\[[[:alnum:]]+\]/);
            name = substr(st, RSTART+1,RLENGTH-2);
            def = substr(st, RSTART+RLENGTH+2);
            Abbrs[toupper(name)] = def;
            return;
        } else if(match(st, /^((    )| *\t)/) || match(st, /^[[:space:]]*```+[[:alnum:]]*/)) {
            Preterm = trim(substr(st, RSTART,RLENGTH));
            st = substr(st, RSTART+RLENGTH);
            if(Buf) res = tag("p", scrub(Buf));
            Buf = st;
            push("pre");
        } else if(!trim(Prev) && match(st, /^[[:space:]]*[*-][[:space:]]*[*-][[:space:]]*[*-][-*[:space:]]*$/)) {
            if(Buf) res = tag("p", scrub(Buf));
            Buf = "";
            res = res "<hr>\n";
        } else if(match(st, /^[[:space:]]*===+[[:space:]]*$/)) {
            Buf = trim(substr(Buf, 1, length(Buf) - length(Prev) - 1));
            if(Buf) res= tag("p", scrub(Buf));
            if(Prev) res = res heading(1, scrub(Prev));
            Buf = "";
        } else if(match(st, /^[[:space:]]*---+[[:space:]]*$/)) {
            Buf = trim(substr(Buf, 1, length(Buf) - length(Prev) - 1));
            if(Buf) res = tag("p", scrub(Buf));
            if(Prev) res = res heading(2, scrub(Prev));
            Buf = "";
        } else if(match(st, /^[[:space:]]*#+/)) {
            sub(/#+[[:space:]]*$/, "", st);
            match(st, /#+/);
            ListLevel = RLENGTH;
            tmp = substr(st, RSTART+RLENGTH);
            if(Buf) res = tag("p", scrub(Buf));
            res = res heading(ListLevel, scrub(trim(tmp)));
            Buf = "";
        } else if(match(st, /^[[:space:]]*>/)) {
            if(Buf) res = tag("p", scrub(Buf));
            Buf = scrub(trim(substr(st, RSTART+RLENGTH)));
            push("blockquote");
        } else if(match(st, /^[[:space:]]*([*+-]|[[:digit:]]+\.)[[:space:]]/)) {
            if(Buf) res = tag("p", scrub(Buf));
            Buf="";
            match(st, /^[[:space:]]*/);
            ListLevel = 1;
            indent[ListLevel] = RLENGTH;
            Open[ListLevel]=match(st, /^[[:space:]]*[*+-][[:space:]]*/)?"ul":"ol";
            push(Open[ListLevel]);
            res = res filter(st);
        } else if(match(st, /^[[:space:]]*$/)) {
            if(trim(Buf)) {
                res = tag("p", scrub(trim(Buf)));
                Buf = "";
            }
        } else
            Buf = Buf st "\n";
        LastLink = 0;
    } else if(Mode == "blockquote") {
        if(match(st, /^[[:space:]]*>[[:space:]]*$/))
            Buf = Buf "\n</p><p>";
        else if(match(st, /^[[:space:]]*>/))
            Buf = Buf "\n" scrub(trim(substr(st, RSTART+RLENGTH)));
        else if(match(st, /^[[:space:]]*$/)) {
            res = tag("blockquote", tag("p", trim(Buf)));
            pop();
            res = res filter(st);
        } else
            Buf = Buf st;
    } else if(Mode == "pre") {
        if(!Preterm && match(st, /^((    )| *\t)/) || Preterm && !match(st, /^[[:space:]]*```+/))
            Buf = Buf ((Buf)?"\n":"") substr(st, RSTART+RLENGTH);
        else {
            gsub(/\t/,"    ",Buf);
            if(length(trim(Buf)) > 0) {
                plang = "";
                if(match(Preterm, /^[[:space:]]*```+/)) {
                    plang = trim(substr(Preterm, RSTART+RLENGTH));
                    if(plang) {
                        plang = "class=\"prettyprint lang-" plang "\"";
                        HasCode=1;
                    }
                }
                res = tag("pre", tag("code", escape(Buf), plang));
            }
            pop();
            if(Preterm) sub(/^[[:space:]]*```+[[:alnum:]]*/,"",st);
            res = res filter(st);
        }
    } else if(Mode == "ul" || Mode == "ol") {
        if(ListLevel == 0 || match(st, /^[[:space:]]*$/) && (RLENGTH <= indent[1])) {
            while(ListLevel > 1)
                Buf = Buf "\n</" Open[ListLevel--] ">";
            res = tag(Mode, "\n" Buf "\n");
            pop();
        } else {
            if(match(st, /^[[:space:]]*([*+-]|[[:digit:]]+\.)/)) {
                tmp = substr(st, RLENGTH+1);
                match(st, /^[[:space:]]*/);
                if(RLENGTH > indent[ListLevel]) {
                    indent[++ListLevel] = RLENGTH;
                    if(match(st, /^[[:space:]]*[*+-]/))
                        Open[ListLevel] = "ul";
                    else
                        Open[ListLevel] = "ol";
                    Buf = Buf "\n<" Open[ListLevel] ">";
                } else while(RLENGTH < indent[ListLevel])
                    Buf = Buf "\n</" Open[ListLevel--] ">";
                if(match(tmp,/^[[:space:]]*\[[xX[:space:]]\]/)) {
                    st = substr(tmp,RLENGTH+1);
                    tmp = tolower(substr(tmp,RSTART,RLENGTH));
                    Buf = Buf "<li><input type=\"checkbox\" " (index(tmp,"x")?"checked":"") " disabled>" scrub(st);
                } else
                    Buf = Buf "<li>" scrub(tmp);
            } else if(match(st, /^[[:space:]]*$/)){
                Buf = Buf "<br>\n";
            } else {
                sub(/^[[:space:]]+/,"",st);
                Buf = Buf "\n" scrub(st);
            }
        }
    }
    Prev = st;
    return res;
}
function scrub(st,    mp, ms, me, r, p, tg, a) {
    sub(/  $/,"<br>\n",st);
    gsub(/(  |[[:space:]]+\\)\n/,"<br>\n",st);
    gsub(/(  |[[:space:]]+\\)$/,"<br>\n",st);
    while(match(st, /(__?|\*\*?|~~|`+|[&><\\])/)) {
        a = substr(st, 1, RSTART-1);
        mp = substr(st, RSTART, RLENGTH);
        ms = substr(st, RSTART-1,1);
        me = substr(st, RSTART+RLENGTH, 1);
        p = RSTART+RLENGTH;

        if(!classic_underscore && match(mp,/_+/)) {
            if(match(ms,/[[:alnum:]]/) && match(me,/[[:alnum:]]/)) {
                tg = substr(st, 1, index(st, mp));
                r = r tg;
                st = substr(st, index(st, mp) + 1);
                continue;
            }
        }
        st = substr(st, p);
        r = r a;
        ms = "";

        if(mp == "\\") {
            if(match(st, /^!?\[/)) {
                r = r "\\" substr(st, RSTART, RLENGTH);
                st = substr(st, 2);
            } else if(match(st, /^(\*\*|__|~~|`+)/)) {
                r = r substr(st, 1, RLENGTH);
                st = substr(st, RLENGTH+1);
            } else {
                r = r substr(st, 1, 1);
                st = substr(st, 2);
            }
            continue;
        } else if(mp == "_" || mp == "*") {
            if(match(me,/[[:space:]]/)) {
                r = r mp;
                continue;
            }
            p = index(st, mp);
            while(p && match(substr(st, p-1, 1),/[\\[:space:]]/)) {
                ms = ms substr(st, 1, p-1) mp;
                st = substr(st, p + length(mp));
                p = index(st, mp);
            }
            if(!p) {
                r = r mp ms;
                continue;
            }
            ms = ms substr(st,1,p-1);
            r = r itag("em", scrub(ms));
            st = substr(st,p+length(mp));
        } else if(mp == "__" || mp == "**") {
            if(match(me,/[[:space:]]/)) {
                r = r mp;
                continue;
            }
            p = index(st, mp);
            while(p && match(substr(st, p-1, 1),/[\\[:space:]]/)) {
                ms = ms substr(st, 1, p-1) mp;
                st = substr(st, p + length(mp));
                p = index(st, mp);
            }
            if(!p) {
                r = r mp ms;
                continue;
            }
            ms = ms substr(st,1,p-1);
            r = r itag("strong", scrub(ms));
            st = substr(st,p+length(mp));
        } else if(mp == "~~") {
            p = index(st, mp);
            if(!p) {
                r = r mp;
                continue;
            }
            while(p && substr(st, p-1, 1) == "\\") {
                ms = ms substr(st, 1, p-1) mp;
                st = substr(st, p + length(mp));
                p = index(st, mp);
            }
            ms = ms substr(st,1,p-1);
            r = r itag("del", scrub(ms));
            st = substr(st,p+length(mp));
        } else if(match(mp, /`+/)) {
            p = index(st, mp);
            if(!p) {
                r = r mp;
                continue;
            }
            ms = substr(st,1,p-1);
            r = r itag("code", escape(ms));
            st = substr(st,p+length(mp));
        } else if(mp == ">") {
            r = r "&gt;";
        } else if(mp == "<") {

            p = index(st, ">");
            if(!p) {
                r = r "&lt;";
                continue;
            }
            tg = substr(st, 1, p - 1);
            if(match(tg,/^[[:alpha:]]+[[:space:]]/)) {
                a = substr(tg,RSTART+RLENGTH-1);
                tg = substr(tg,1,RLENGTH-1);
            } else
                a = "";

            if(match(tolower(tg), "^/?(a|abbr|div|span|blockquote|pre|img|code|p|em|strong|sup|sub|del|ins|s|u|b|i|br|hr|ul|ol|li|table|thead|tfoot|tbody|tr|th|td|caption|column|col|colgroup|figure|figcaption|dl|dd|dt|mark|cite|q|var|samp|small|details|summary)$")) {
                r = r "<" tg a ">";
            } else if(match(tg, "^[[:alpha:]]+://[[:graph:]]+$")) {
                if(!a) a = tg;
                r = r "<a class=\"normal\" href=\"" tg "\">" a "</a>";
            } else if(match(tg, "^[[:graph:]]+@[[:graph:]]+$")) {
                if(!a) a = tg;
                r = r "<a class=\"normal\" href=\"" obfuscate("mailto:" tg) "\">" obfuscate(a) "</a>";
            } else {
                r = r "&lt;";
                continue;
            }

            st = substr(st, p + 1);
        } else if(mp == "&") {
            if(match(st, /^[#[:alnum:]]+;/)) {
                r = r "&" substr(st, 1, RLENGTH);
                st = substr(st, RLENGTH+1);
            } else {
                r = r "&amp;";
            }
        }
    }
    return r st;
}

function push(newmode) {Stack[StackTop++] = Mode; Mode = newmode;}
function pop() {Mode = Stack[--StackTop];Buf = ""; return Mode;}
function heading(level, st,       res, href, u, text,svg) {
    if(level > 6) level = 6;
    st = trim(st);
    href = tolower(st);
    href = strip_tags(href);
    gsub(/[^-_ [:alnum:]]+/, "", href);
    gsub(/[[:space:]]/, "-", href);
    if(TitleUrls[href]) {
        for(u = 1; TitleUrls[href "-" u]; u++);
        href = href "-" u;
    }
    TitleUrls[href] = "#" href;

    svg = "<svg width=\"16\" height=\"16\" xmlns=\"http://www.w3.org/2000/svg\"><g transform=\"rotate(-30, 8, 8)\" stroke=\"#000000\" opacity=\"0.25\"><rect fill=\"none\" height=\"6\" width=\"8\" x=\"2\" y=\"6\" rx=\"1.5\"/><rect fill=\"none\" height=\"6\" width=\"8\" x=\"6\" y=\"4\" rx=\"1.5\"/></g></svg>";
    text = "<a href=\"#" href "\" class=\"header\">" st "&nbsp;" svg "</a>" (TopLinks?"&nbsp;&nbsp;<a class=\"top\" title=\"Return to top\" href=\"#\">&#8593;&nbsp;Top</a>":"");

    res = tag("h" level, text, "id=\"" href "\"");
    for(;ToCLevel < level; ToCLevel++) {
        ToC_ID++;
        if(ToCLevel < HideToCLevel) {
            ToC = ToC "<a class=\"toc-button\" id=\"toc-btn-" ToC_ID "\" onclick=\"toggle_toc_ul('" ToC_ID "')\">&#x25BC;</a>";
            ToC = ToC "<ul class=\"toc toc-" ToCLevel "\" id=\"toc-ul-" ToC_ID "\">";
        } else {
            ToC = ToC "<a class=\"toc toc-button\" id=\"toc-btn-" ToC_ID "\" onclick=\"toggle_toc_ul('" ToC_ID "')\">&#x25BA;</a>";
            ToC = ToC "<ul style=\"display:none;\" class=\"toc toc-" ToCLevel "\" id=\"toc-ul-" ToC_ID "\">";
        }
    }
    for(;ToCLevel > level; ToCLevel--)
        ToC = ToC "</ul>";
    ToC = ToC "<li class=\"toc-" level "\"><a class=\"toc toc-" level "\" href=\"#" href "\">" st "</a>\n";
    ToCLevel = level;
    return res;
}
function make_toc(st,              r,p,dis,t,n) {
    if(!ToC) return st;
    for(;ToCLevel > 1;ToCLevel--)
        ToC = ToC "</ul>";
    p = match(st, /!\[toc[-+]?\]/);
    while(p) {
        if(substr(st,RSTART-1,1) == "\\") {
            r = r substr(st,1,RSTART-2) substr(st,RSTART,RLENGTH);
            st = substr(st,RSTART+RLENGTH);
            p = match(st, /!\[toc[-+]?\]/);
            continue;
        }

        ++n;
        dis = index(substr(st,RSTART,RLENGTH),"+");
        t = "<div>\n<a id=\"toc-button-" n "\" class=\"toc-button\" onclick=\"toggle_toc(" n ")\"><span id=\"btn-text-" n "\">" (dis?"&#x25BC;":"&#x25BA;") "</span>&nbsp;Contents</a>\n" \
            "<div id=\"table-of-contents-" n "\" style=\"display:" (dis?"block":"none") ";\">\n<ul class=\"toc toc-1\">" ToC "</ul>\n</div>\n</div>";
        r = r substr(st,1,RSTART-1);
        r = r t;
        st = substr(st,RSTART+RLENGTH);
        p = match(st, /!\[toc[-+]?\]/);
    }
    return r st;
}
function fix_links(st,          lt,ld,lr,url,img,res,rx,pos,pre) {
    do {
        pre = match(st, /<(pre|code)>/); # Don't substitute in <pre> or <code> blocks
        pos = match(st, /\[[^\]]+\]/);
        if(!pos)break;
        if(pre && pre < pos) {
            match(st, /<\/(pre|code)>/);
            res = res substr(st,1,RSTART+RLENGTH);
            st = substr(st, RSTART+RLENGTH+1);
            continue;
        }
        img=substr(st,RSTART-1,1)=="!";
        if(substr(st, RSTART-(img?2:1),1)=="\\") {
            res = res substr(st,1,RSTART-(img?3:2));
            if(img && substr(st,RSTART,RLENGTH)=="[toc]")res=res "\\";
            res = res substr(st,RSTART-(img?1:0),RLENGTH+(img?1:0));
            st = substr(st, RSTART + RLENGTH);
            continue;
        }
        res = res substr(st, 1, RSTART-(img?2:1));
        rx = substr(st, RSTART, RLENGTH);
        st = substr(st, RSTART+RLENGTH);
        if(match(st, /^[[:space:]]*\([^)]+\)/)) {
            lt = substr(rx, 2, length(rx) - 2);
            match(st, /\([^)]+\)/);
            url = substr(st, RSTART+1, RLENGTH-2);
            st = substr(st, RSTART+RLENGTH);
            ld = "";
            if(match(url,/[[:space:]]+["']/)) {
                ld = url;
                url = substr(url, 1, RSTART - 1);
                match(ld,/["']/);
                delim = substr(ld, RSTART, 1);
                if(match(ld,delim ".*" delim))
                    ld = substr(ld, RSTART+1, RLENGTH-2);
            }  else ld = "";
            if(img)
                res = res "<img src=\"" url "\" title=\"" ld "\" alt=\"" lt "\">";
            else
                res = res "<a class=\"normal\" href=\"" url "\" title=\"" ld "\">" lt "</a>";
        } else if(match(st, /^[[:space:]]*\[[^\]]*\]/)) {
            lt = substr(rx, 2, length(rx) - 2);
            match(st, /\[[^\]]*\]/);
            lr = trim(tolower(substr(st, RSTART+1, RLENGTH-2)));
            if(!lr) {
                lr = tolower(trim(lt));
                if(LinkDescs[lr]) lt = LinkDescs[lr];
            }
            st = substr(st, RSTART+RLENGTH);
            url = LinkUrls[lr];
            ld = LinkDescs[lr];
            if(img)
                res = res "<img src=\"" url "\" title=\"" ld "\" alt=\"" lt "\">";
            else if(url)
                res = res "<a class=\"normal\" href=\"" url "\" title=\"" ld "\">" lt "</a>";
            else
                res = res "[" lt "][" lr "]";
        } else
            res = res (img?"!":"") rx;
    } while(pos > 0);
    return res st;
}
function fix_footnotes(st,         r,p,n,i,d,fn,fc) {
    p = match(st, /\[\^[^\]]+\]/);
    while(p) {
        if(substr(st,RSTART-2,1) == "\\") {
            r = r substr(st,1,RSTART-3) substr(st,RSTART,RLENGTH);
            st = substr(st,RSTART+RLENGTH);
            p = match(st, /\[\^[^\]]+\]/);
            continue;
        }
        r = r substr(st,1,RSTART-1);
        d = substr(st,RSTART+2,RLENGTH-3);
        n = tolower(d);
        st = substr(st,RSTART+RLENGTH);
        if(Footnote[tolower(n)]) {
            if(!fn[n]) fn[n] = ++fc;
            d = Footnote[n];
        } else {
            Footnote[n] = scrub(d);
            if(!fn[n]) fn[n] = ++fc;
        }
        footname[fc] = n;
        d = strip_tags(d);
        if(length(d) > 20) d = substr(d,1,20) "&hellip;";
        r = r "<sup title=\"" d "\"><a href=\"#footnote-" fn[n] "\" id=\"footnote-pos-" fn[n] "\" class=\"footnote\">[" fn[n] "]</a></sup>";
        p = match(st, /\[\^[^\]]+\]/);
    }
    for(i=1;i<=fc;i++)
        footnotes = footnotes "<li id=\"footnote-" i "\">" Footnote[footname[i]] \
            "<a title=\"Return to Document\" class=\"footnote-back\" href=\"#footnote-pos-" i \
            "\">&nbsp;&nbsp;&#8630;&nbsp;Back</a></li>\n";
    return r st;
}
function fix_abbrs(str,         st,k,r,p) {
    for(k in Abbrs) {
        r = "";
        st = str;
        t = escape(Abbrs[toupper(k)]);
        gsub(/&/,"\\&", t);
        p = match(st,"[^[:alnum:]]" k "[^[:alnum:]]");
        while(p) {
            r = r substr(st, 1, RSTART);
            r = r "<abbr title=\"" t "\">" k "</abbr>";
            st = substr(st, RSTART+RLENGTH-1);
            p = match(st,"[^[:alnum:]]" k "[^[:alnum:]]");
        }
        str = r st;
    }
    return str;
}
function tag(t, body, attr) {
    if(attr)
        attr = " " trim(attr);
    # https://www.w3.org/TR/html5/grouping-content.html#the-p-element
    if(t == "p" && (match(body, /<\/?(div|table|blockquote|dl|ol|ul|h[[:digit:]]|hr|pre)[>[:space:]]/))|| (match(body,/!\[toc\]/) && substr(body, RSTART-1,1) != "\\"))
        return "<" t attr ">" body "\n";
    else
        return "<" t attr ">" body "</" t ">\n";
}
function itag(t, body) {
    return "<" t ">" body "</" t ">";
}
function obfuscate(e,     r,i,t,o) {
    for(i = 1; i <= length(e); i++) {
        t = substr(e,i,1);
        r = int(rand() * 100);
        if(r > 50)
            o = o sprintf("&#x%02X;", _ord[t]);
        else if(r > 10)
            o = o sprintf("&#%d;", _ord[t]);
        else
            o = o t;
    }
    return o;
}
function init_css(Theme,             css,ss,hr,c1,c2,c3,c4,c5,bg1,bg2,bg3,bg4,ff,fs,i) {
    if(Theme == "0") return "";

    css["body"] = "color:%color1%;font-family:%font-family%;font-size:%font-size%;line-height:1.5em;" \
                "padding:1em 2em;width:80%;max-width:%maxwidth%;margin:0 auto;min-height:100%;float:none;";
    css["h1"] = "border-bottom:1px solid %color1%;padding:0.3em 0.1em;";
    css["h1 a"] = "color:%color1%;";
    css["h2"] = "color:%color2%;border-bottom:1px solid %color2%;padding:0.2em 0.1em;";
    css["h2 a"] = "color:%color2%;";
    css["h3"] = "color:%color3%;border-bottom:1px solid %color3%;padding:0.1em 0.1em;";
    css["h3 a"] = "color:%color3%;";
    css["h4,h5,h6"] = "padding:0.1em 0.1em;";
    css["h4 a,h5 a,h6 a"] = "color:%color4%;";
    css["h1,h2,h3,h4,h5,h6"] = "font-weight:bolder;line-height:1.2em;";
    css["h4"] = "border-bottom:1px solid %color4%";
    css["p"] = "margin:0.5em 0.1em;"
    css["hr"] = "background:%color1%;height:1px;border:0;"
    css["a.normal, a.toc"] = "color:%color2%;";
    #css["a.normal:visited"] = "color:%color2%;";
    #css["a.normal:active"] = "color:%color4%;";
    css["a.normal:hover, a.toc:hover"] = "color:%color4%;";
    css["a.top"] = "font-size:x-small;text-decoration:initial;float:right;";
    css["a.header svg"] = "opacity:0;";
    css["a.header:hover svg"] = "opacity:1;";
    css["a.header"] = "text-decoration: none;";
    css["strong,b"] = "color:%color1%";
    css["code"] = "color:%color2%;font-weight:bold;";
    css["blockquote"] = "margin-left:1em;color:%color2%;border-left:0.2em solid %color3%;padding:0.25em 0.5em;overflow-x:auto;";
    css["pre"] = "color:%color2%;background:%color5%;border:1px solid;border-radius:2px;line-height:1.25em;margin:0.25em 0.5em;padding:0.75em;overflow-x:auto;";
    css["table"] = "border-collapse:collapse;margin:0.5em;";
    css["th,td"] = "padding:0.5em 0.75em;border:1px solid %color4%;";
    css["th"] = "color:%color2%;border:1px solid %color3%;border-bottom:2px solid %color3%;";
    css["tr:nth-child(odd)"] = "background-color:%color5%;";
    css["div"] = "padding:0.5em;";
    css["caption"] = "padding:0.5em;font-style:italic;";
    css["dl"] = "margin:0.5em;";
    css["dt"] = "font-weight:bold;";
    css["dd"] = "padding:0.3em;";
    css["mark"] = "color:%color5%;background-color:%color4%;";
    css["del,s"] = "color:%color4%;";
    css["a.toc-button"] = "color:%color2%;cursor:pointer;font-size:small;padding: 0.3em 0.5em 0.5em 0.5em;font-family:monospace;border-radius:3px;";
    css["a.toc-button:hover"] = "color:%color4%;background:%color5%;";
    css["div#table-of-contents"] = "padding:0;font-size:smaller;";
    css["abbr"] = "cursor:help;";
    css["ol.footnotes"] = "font-size:small;color:%color4%";
    css["a.footnote"] = "font-size:smaller;text-decoration:initial;";
    css["a.footnote-back"] = "text-decoration:initial;font-size:x-small;";
    css[".fade"] = "color:%color5%;";
    css[".highlight"] = "color:%color2%;background-color:%color5%;";
	css["summary"] = "cursor:pointer;";
	css["ul.toc"] = "list-style-type:none;";

    if(NumberHeadings)  {
        if(NumberH1s) {
            css["body"] = css["body"] "counter-reset: h1 toc1;";
            css["h1"] = css["h1"] "counter-reset: h2 h3 h4;";
            css["h2"] = css["h2"] "counter-reset: h3 h4;";
            css["h3"] = css["h3"] "counter-reset: h4;";
            css["h1::before"] = "content: counter(h1) \" \"; counter-increment: h1; margin-right: 10px;";
            css["h2::before"] = "content: counter(h1) \".\"counter(h2) \" \";counter-increment: h2; margin-right: 10px;";
            css["h3::before"] = "content: counter(h1) \".\"counter(h2) \".\"counter(h3) \" \";counter-increment: h3; margin-right: 10px;";
            css["h4::before"] = "content: counter(h1) \".\"counter(h2) \".\"counter(h3)\".\"counter(h4) \" \";counter-increment: h4; margin-right: 10px;";

            css["li.toc-1"] = "counter-reset: toc2 toc3 toc4;";
            css["li.toc-2"] = "counter-reset: toc3 toc4;";
            css["li.toc-3"] = "counter-reset: toc4;";
            css["a.toc-1::before"] = "content: counter(h1) \"  \";counter-increment: toc1;";
            css["a.toc-2::before"] = "content: counter(h1) \".\" counter(toc2) \"  \";counter-increment: toc2;";
            css["a.toc-3::before"] = "content: counter(h1) \".\" counter(toc2) \".\" counter(toc3) \"  \";counter-increment: toc3;";
            css["a.toc-4::before"] = "content: counter(h1) \".\" counter(toc2) \".\" counter(toc3) \".\" counter(toc4) \"  \";counter-increment: toc4;";

        } else {
            css["h1"] = css["h1"] "counter-reset: h2 h3 h4;";
            css["h2"] = css["h2"] "counter-reset: h3 h4;";
            css["h3"] = css["h3"] "counter-reset: h4;";
            css["h2::before"] = "content: counter(h2) \" \";counter-increment: h2; margin-right: 10px;";
            css["h3::before"] = "content: counter(h2) \".\"counter(h3) \" \";counter-increment: h3; margin-right: 10px;";
            css["h4::before"] = "content: counter(h2) \".\"counter(h3)\".\"counter(h4) \" \";counter-increment: h4; margin-right: 10px;";

            css["li.toc-1"] = "counter-reset: toc2 toc3 toc4;";
            css["li.toc-2"] = "counter-reset: toc3 toc4;";
            css["li.toc-3"] = "counter-reset: toc4;";
            css["a.toc-2::before"] = "content: counter(toc2) \"  \";counter-increment: toc2;";
            css["a.toc-3::before"] = "content: counter(toc2) \".\" counter(toc3) \"  \";counter-increment: toc3;";
            css["a.toc-4::before"] = "content: counter(toc2) \".\" counter(toc3) \".\" counter(toc4) \"  \";counter-increment: toc4;";
        }
    }

    # Colors:
    #c1="#314070";c2="#465DA6";c3="#6676A8";c4="#A88C3F";c5="#E8E4D9";
    c1="#314070";c2="#384877";c3="#6676A8";c4="#738FD0";c5="#FBFCFF";
    # Font Family:
    ff = "sans-serif";
    fs = "11pt";

    # Alternative color scheme suggestions:
    #c1="#303F9F";c2="#0449CC";c3="#2162FA";c4="#4B80FB";c5="#EDF2FF";
    #ff="\"Trebuchet MS\", Helvetica, sans-serif";
    #c1="#430005";c2="#740009";c3="#A6373F";c4="#c55158";c5="#fbf2f2";
    #ff="Verdana, Geneva, sans-serif";
    #c1="#083900";c2="#0D6300";c3="#3C8D2F";c4="#50be3f";c5="#f2faf1";
    #ff="Georgia, serif";
    #c1="#35305D";c2="#646379";c3="#7A74A5";c4="#646392";c5="#fafafa";

    for(i = 0; i<=255; i++)_hex[sprintf("%02X",i)]=i;
    for(k in css)
        ss = ss "\n" k "{" css[k] "}";
    gsub(/%maxwidth%/,MaxWidth,ss);
    gsub(/%color1%/,c1,ss);
    gsub(/%color2%/,c2,ss);
    gsub(/%color3%/,c3,ss);
    gsub(/%color4%/,c4,ss);
    gsub(/%color5%/,c5,ss);
    gsub(/%font-family%/,ff,ss);
    gsub(/%font-size%/,fs,ss);
    gsub(/%hr%/,hr,ss);
    return ss;
}
