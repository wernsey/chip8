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
# - `-vCss=n` with n either 0 to disable CSS or 1 to enable; Default = 1
# - `-vTopLinks=1` to have links to the top of the doc next to headers.
# - `-vMaxWidth=1080px` specifies the Maximum width of the HTML output.
# - `-vPretty=0` disable syntax highlighting with Google's [code prettify][].
# - `-vMermaid=0` disable [Mermaid][] diagrams.
# - `-vMathjax=0` disable [MathJax][] mathematical rendering.
# - `-vHideToCLevel=n` specifies the level of the ToC that should be collapsed by default.
# - `-vLang=n` specifies the value of the `lang` attribute of the <html> tag; Default = "en"
# - `-vTables=0` disables support for [GitHub-style tables][github-tables]
# - `-vclassic_underscore=1` words_with_underscores behave like old markdown where the
#        underscores in the word counts as emphasis. The default behaviour is to have
#        `words_like_this` not contain any emphasis.
# - `-vNumberHeadings=1` to enable or disable section numbers in front of headings; Default = 1
# - `-vNumberH1s=1`: if `NumberHeadings` is enabled, `<H1>` headings are not numbered by
#        default (because the `<H1>` would typically contain the document title). Use this to
#        number `<H1>`s as well.
# - `-vClean=1` to treat the input file as a plain Markdown file.
#        You could use `./d.awk -vClean=1 README.md > README.html` to generate HTML from
#        your README, for example.
#
# I've tested it with Gawk, Nawk and Mawk.
# Gawk and Nawk worked without issues, but don't use the `-W compat`
# or `-W traditional` settings with Gawk.
# Mawk v1.3.4 worked correctly but v1.3.3 choked on it.
#
# [code prettify]: https://github.com/google/code-prettify
# [Mermaid]: https://github.com/mermaid-js/mermaid
# [MathJax]: https://www.mathjax.org/
# [github-tables]: https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet#tables
# [github-mermaid]: https://github.blog/2022-02-14-include-diagrams-markdown-files-mermaid/
# [github-math]: https://github.blog/changelog/2022-05-19-render-mathematical-expressions-in-markdown/
#
# ## Extensions
#
# - Insert a Table of Contents by using `\![toc]`.
#   The Table of Contents is collapsed by default:
#   - Use `\\![toc+]` to insert a ToC that is expanded by default;
#   - Use `\\![toc-]` for a collapsed ToC.
# - Github-style ```` ``` ```` code blocks supported.
# - Github-style `~~strikethrough~~` supported.
# - [Github-style tables][github-tables] are supported.
# - [GitHub-style Mermaid diagrams][github-mermaid]
# - [GitHub-style mathematical expressions][github-math]: $\sqrt{3x-1}+(1+x)^2$
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
#     (c) 2016-2023 Werner Stoop
#     Copying and distribution of this file, with or without modification,
#     are permitted in any medium without royalty provided the copyright
#     notice and this notice are preserved. This file is offered as-is,
#     without any warranty.
#

BEGIN {

    # Configuration options
    if(Title== "") Title = "Documentation";
    if(Css== "") Css = 1;

    if(Pretty== "") Pretty = 1;
    if(Mermaid== "") Mermaid = 1;
    if(Mathjax=="") Mathjax = 1;

    if(HideToCLevel== "") HideToCLevel = 3;
    if(Lang == "") Lang = "en";
    if(Tables == "") Tables = 1;
    #TopLinks = 1;
    #classic_underscore = 1;
    if(MaxWidth=="") MaxWidth="1080px";
    if(NumberHeadings=="") NumberHeadings = 1;
    if(NumberH1s=="") NumberH1s = 0;

    Mode = (Clean)?"p":"none";
    ToC = ""; ToCLevel = 1;
    CSS = init_css(Css);
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
    } else if(Mode == "table") {
        Out = Out end_table();
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
    } else if(Mode == "table") {
        Out = Out end_table();
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
    else {
        if(Pretty && HasPretty) {
            CSS = CSS "\nbody {--str:#a636d8;--kwd:#4646ff;--com:#56a656;--lit:#e05e10;--typ:#0222ce;--pun:#595959;}\n"\
                "body.dark-theme {--str:#eb28df;--kwd:#f7d689;--com:#267b26;--lit: #ff8181;--typ:#228dff;--pun: #EEE;}\n"\
                "@media (prefers-color-scheme: dark) {\n"\
                "    body.light-theme {--str:#a636d8;--kwd:#4646ff;--com:#56a656;--lit:#e05e10;--typ:#0222ce;--pun:#595959;}\n"\
                "    body {--str:#eb28df;--kwd:#f7d689;--com:#267b26;--lit: #ff8181;--typ:#228dff;--pun: #EEE;}\n"\
                "}\n"\
                ".com { color:var(--com); } /* comment */\n"\
                ".kwd, .tag { color:var(--kwd); } /* keyword, markup tag */\n"\
                ".typ, .atn { color:var(--typ); } /* type name, html/xml attribute name */\n"\
                ".str, .atv { color:var(--str); } /* string literal, html/xml attribute value */\n"\
                ".lit, .dec, .var { color:var(--lit); } /* literal */\n"\
                ".pun, .opn, .clo { color:var(--pun); } /* punctuation */\n"\
                ".pln { color:var(--alt-color); } /* plain text */\n"\
                "@media print, projection {\n"\
                "    .com { font-style: italic }\n"\
                "    .kwd, .typ, .tag { font-weight: bold }\n"\
                "}";
        }
        print "<style><!--" CSS "\n" \
        ".print-only {display:none}\n"\
        "@media print { .no-print { display: none !important; } .print-only {display:block} }\n" \
        "--></style>";
    }
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
    print "</head><body onload=\"PR.prettyPrint()\">";

    print "<a class=\"dark-toggle no-print\">\n" \
        "<svg width=\"12\" height=\"12\" viewBox=\"0 0 12 20\" xmlns=\"http://www.w3.org/2000/svg\">\n" \
            "<g transform=\"translate(8 12) scale(8 8)\">\n" \
                "<path fill=\"var(--color)\" d=\"M 0.25 -1 C -0.5 -1 -1 -0.5 -1 0 C -1.02 0.5 -0.5 1 0.25 1 C 0 1 -0.5 0.5 -0.5 0 C -0.5 -0.5 0 -1 0.25 -1\"/>\n" \
            "</g>\n" \
        "</svg>\n&nbsp;Toggle Dark Mode</a>\n";
    print "<script>\n"\
    "const prefersDarkScheme = window.matchMedia('(prefers-color-scheme: dark)');\n"\
    "document.querySelector('.dark-toggle').addEventListener('click', function () {\n"\
    "    document.body.classList.toggle(prefersDarkScheme.matches ? 'light-theme' : 'dark-theme');\n"\
    "});\n"\
    "</script>";

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

    if(Pretty && HasPretty) {
        print "<script src=\"https://cdn.jsdelivr.net/gh/google/code-prettify@master/loader/prettify.js\"></script>";
    }
    if(Mermaid && HasMermaid) {
        print "<script src=\"https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js\"></script>";
        print "<script>mermaid.initialize({ startOnLoad: true, theme:window.matchMedia('(prefers-color-scheme: dark)').matches?'dark':'default'  });</script>";
    }
    if(Mathjax && HasMathjax) {
        print "<script>MathJax={tex:{inlineMath:[['$','$'],['\\\\(','\\\\)']]},svg:{fontCache:'global'}};</script>";
        print "<script src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-svg.js\" type=\"text/javascript\" id=\"MathJax-script\" async></script>";
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
function filter(st,       res,tmp, linkdesc, url, delim, edelim, name, def, plang, mmaid, cols, i) {
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
        } else if(Tables && match(st, /.*\|(.*\|)+/)) {
            if(Buf) res = tag("p", scrub(Buf));
            Row = 1;
            for(i = 1; i <= MaxCols; i++)
                Align[i] = "";
            process_table_row(st);
            push("table");
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
    } else if(Mode == "table") {
        if(match(st, /.*\|(.*\|)+/)) {
            process_table_row(st);
        } else {
            res = end_table();
            pop();
            res = res filter(st);
        }
    } else if(Mode == "pre") {
        if(!Preterm && match(st, /^((    )| *\t)/) || Preterm && !match(st, /^[[:space:]]*```+/))
            Buf = Buf ((Buf)?"\n":"") substr(st, RSTART+RLENGTH);
        else {
            gsub(/\t/,"    ",Buf);
            if(length(trim(Buf)) > 0) {
                plang = ""; mmaid=0;
                if(match(Preterm, /^[[:space:]]*```+/)) {
                    plang = trim(substr(Preterm, RSTART+RLENGTH));
                    if(plang) {
                        if(plang == "mermaid") {
                            mmaid = 1;
                            HasMermaid = 1;
                        } else {
                            HasPretty = 1;
                            if(plang == "auto")
                                plang = "class=\"prettyprint\"";
                            else
                                plang = "class=\"prettyprint lang-" plang "\"";
                        }
                    }
                }
                if(mmaid && Mermaid)
                    res = tag("div", Buf, "class=\"mermaid\"");
                else
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
function scrub(st,    mp, ms, me, r, p, tg, a, tok) {
    sub(/  $/,"<br>\n",st);
    gsub(/(  |[[:space:]]+\\)\n/,"<br>\n",st);
    gsub(/(  |[[:space:]]+\\)$/,"<br>\n",st);
    while(match(st, /(__?|\*\*?|~~|`+|\$+|\\\(|[&><\\])/)) {
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
        } else if(Mathjax && match(mp, /\$+/)) {
            tok = substr(mp, RSTART, RLENGTH);
            p = index(st, mp);
            if(!p) {
                r = r mp;
                continue;
            }
            ms = substr(st,1,p-1);
            r = r tok escape(ms) tok;
            st = substr(st,p+length(mp));
            HasMathjax = 1;
        } else if(Mathjax && mp=="\\(") {
            p = index(st, "\\)");
            if(!p) {
                r = r mp;
                continue;
            }
            ms = substr(st,1,p-1);
            r = r "\\(" escape(ms) "\\)";
            st = substr(st,p+length(mp));
            HasMathjax = 1;
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
                a = trim(substr(tg,RSTART+RLENGTH-1));
                tg = substr(tg,1,RLENGTH-1);
            } else
                a = "";

            if(match(tolower(tg), "^/?(a|abbr|div|span|blockquote|pre|img|code|p|em|strong|sup|sub|del|ins|s|u|b|i|br|hr|ul|ol|li|table|thead|tfoot|tbody|tr|th|td|caption|column|col|colgroup|figure|figcaption|dl|dd|dt|mark|cite|q|var|samp|small|details|summary)$")) {
                if(!match(tg, /\//)) {
                    if(match(a, /class="/)) {
                        sub(/class="/, "class=\"dawk-ex ", a);
                    } else {
                        if(a)
                            a = a " class=\"dawk-ex\""
                        else
                            a = "class=\"dawk-ex\""
                    }
                    r = r "<" tg " " a ">";
                } else
                    r = r "<" tg ">";
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
            ToC = ToC "<a class=\"toc-button no-print\" id=\"toc-btn-" ToC_ID "\" onclick=\"toggle_toc_ul('" ToC_ID "')\">&#x25BC;</a>";
            ToC = ToC "<ul class=\"toc toc-" ToCLevel "\" id=\"toc-ul-" ToC_ID "\">";
        } else {
            ToC = ToC "<a class=\"toc toc-button no-print\" id=\"toc-btn-" ToC_ID "\" onclick=\"toggle_toc_ul('" ToC_ID "')\">&#x25BA;</a>";
            ToC = ToC "<ul style=\"display:none;\" class=\"toc toc-" ToCLevel "\" id=\"toc-ul-" ToC_ID "\">";
        }
    }
    for(;ToCLevel > level; ToCLevel--)
        ToC = ToC "</ul>";
    ToC = ToC "<li class=\"toc-" level "\"><a class=\"toc toc-" level "\" href=\"#" href "\">" st "</a>\n";
    ToCLevel = level;
    return res;
}
function process_table_row(st       ,cols, i) {
    if(match(st, /^[[:space:]]*\|/))
        st = substr(st, RSTART+RLENGTH);
    if(match(st, /\|[[:space:]]*$/))
        st = substr(st, 1, RSTART - 1);
    st = trim(st);

    if(match(st, /^([[:space:]:|]|---+)*$/)) {
        IsHeaders[Row-1] = 1;
        cols = split(st, A, /[[:space:]]*\|[[:space:]]*/)
        for(i = 1; i <= cols; i++) {
            if(match(A[i], /^:-*:$/))
                Align[i] = "center";
            else if(match(A[i], /^-*:$/))
                Align[i] = "right";
            else if(match(A[i], /^:-*$/))
                Align[i] = "left";
        }
        return;
    }

    cols = split(st, A, /[[:space:]]*\|[[:space:]]*/);
    for(i = 1; i <= cols; i++) {
        Table[Row, i] = A[i];
    }
    NCols[Row] = cols;
    if(cols > MaxCols)
        MaxCols = cols;
    IsHeaders[Row] = 0;
    Row++;
}
function end_table(         r,c,t,a,s) {
    for(r = 1; r < Row; r++) {
        t = IsHeaders[r] ? "th" : "td"
        s = s "<tr>"
        for(c = 1; c <= NCols[r]; c++) {
            a = Align[c];
            if(a)
                s = s "<" t " align=\"" a "\">" scrub(Table[r,c]) "</" t ">"
            else
                s = s "<" t ">" scrub(Table[r,c]) "</" t ">"
        }
        s = s "</tr>\n"
    }
    return tag("table", s, "class=\"da\"");
}
function make_toc(st,              r,p,dis,t,n,tocBody) {
    if(!ToC) return st;
    for(;ToCLevel > 1;ToCLevel--)
        ToC = ToC "</ul>";

    tocBody = "<ul class=\"toc toc-1\">" ToC "</ul>\n";

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
        t = "<details id=\"table-of-contents\" class=\"no-print\">\n<summary id=\"toc-button-" n "\" class=\"toc-button\">Contents</summary>\n" \
            tocBody "</details>";
        t = t "\n<div class=\"print-only\">" tocBody "</div>"
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
function init_css(Css,             css,ss,hr,bg1,bg2,bg3,bg4,ff,fs,i,lt,dt) {
    if(Css == "0") return "";

    css["body"] = "color:var(--color);background:var(--background);font-family:%font-family%;font-size:%font-size%;line-height:1.5em;" \
                "padding:1em 2em;width:80%;max-width:%maxwidth%;margin:0 auto;min-height:100%;float:none;";
    css["h1"] = "border-bottom:1px solid var(--heading);padding:0.3em 0.1em;";
    css["h1 a"] = "color:var(--heading);";
    css["h2"] = "color:var(--heading);border-bottom:1px solid var(--heading);padding:0.2em 0.1em;";
    css["h2 a"] = "color:var(--heading);";
    css["h3"] = "color:var(--heading);border-bottom:1px solid var(--heading);padding:0.1em 0.1em;";
    css["h3 a"] = "color:var(--heading);";
    css["h4,h5,h6"] = "padding:0.1em 0.1em;";
    css["h4 a,h5 a,h6 a"] = "color:var(--heading);";
    css["h1,h2,h3,h4,h5,h6"] = "font-weight:bolder;line-height:1.2em;";
    css["h4"] = "border-bottom:1px solid var(--heading)";
    css["p"] = "margin:0.5em 0.1em;"
    css["hr"] = "background:var(--color);height:1px;border:0;"
    css["a.normal, a.toc"] = "color:var(--alt-color);";
    #css["a.normal:visited"] = "color:var(--heading);";
    #css["a.normal:active"] = "color:var(--heading);";
    css["a.normal:hover, a.toc:hover"] = "color:var(--alt-color);";
    css["a.top"] = "font-size:x-small;text-decoration:initial;float:right;";
    css["a.header svg"] = "opacity:0;";
    css["a.header:hover svg"] = "opacity:1;";
    css["a.header"] = "text-decoration: none;";
    css["a.dark-toggle"] = "float:right; cursor: pointer; font-size: small; padding: 0.3em 0.5em 0.5em 0.5em; font-family: monospace; border-radius: 3px;";
    css["a.dark-toggle:hover"] = "background:var(--alt-background);";
    css[".toc-button"] = "color:var(--alt-color);cursor:pointer;font-size:small;padding: 0.3em 0.5em 0.5em 0.5em;font-family:monospace;border-radius:3px;";
    css["a.toc-button:hover"] = "background:var(--alt-background);";
    css["a.footnote"] = "font-size:smaller;text-decoration:initial;";
    css["a.footnote-back"] = "text-decoration:initial;font-size:x-small;";
    css["strong,b"] = "color:var(--color)";
    css["code"] = "color:var(--alt-color);font-weight:bold;";
    css["blockquote"] = "margin-left:1em;color:var(--alt-color);border-left:0.2em solid var(--alt-color);padding:0.25em 0.5em;overflow-x:auto;";
    css["pre"] = "color:var(--alt-color);background:var(--alt-background);border:1px solid;border-radius:2px;line-height:1.25em;margin:0.25em 0.5em;padding:0.75em;overflow-x:auto;";
    css["table.dawk-ex"] = "border-collapse:collapse;margin:0.5em;";
    css["th.dawk-ex,td.dawk-ex"] = "padding:0.5em 0.75em;border:1px solid var(--heading);";
    css["th.dawk-ex"] = "color:var(--heading);border:1px solid var(--heading);border-bottom:2px solid var(--heading);";
    css["tr.dawk-ex:nth-child(odd)"] = "background-color:var(--alt-background);";
    css["table.da"] = "border-collapse:collapse;margin:0.5em;";
    css["table.da th,td"] = "padding:0.5em 0.75em;border:1px solid var(--heading);";
    css["table.da th"] = "color:var(--heading);border:1px solid var(--heading);border-bottom:2px solid var(--heading);";
    css["table.da tr:nth-child(odd)"] = "background-color:var(--alt-background);";
    css["div.dawk-ex"] = "padding:0.5em;";
    css["caption.dawk-ex"] = "padding:0.5em;font-style:italic;";
    css["dl.dawk-ex"] = "margin:0.5em;";
    css["dt.dawk-ex"] = "font-weight:bold;";
    css["dd.dawk-ex"] = "padding:0.3em;";
    css["mark.dawk-ex"] = "color:var(--alt-background);background-color:var(--heading);";
    css["del.dawk-ex,s.dawk-ex"] = "color:var(--heading);";
    css["div#table-of-contents"] = "padding:0;font-size:smaller;";
    css["abbr"] = "cursor:help;";
    css["ol.footnotes"] = "font-size:small;color:var(--alt-color)";
    css[".fade"] = "color:var(--alt-background);";
    css[".highlight"] = "color:var(--alt-color);background-color:var(--alt-background);";
    css["summary"] = "cursor:pointer;";
    css["ul.toc"] = "list-style-type:none;";

    # This is a trick to prevent page-breaks immediately after headers
    # https://stackoverflow.com/a/53742871/115589
    css["blockquote,code,pre,table"] = "break-inside: avoid;break-before: auto;"
    css["section"] = "break-inside: avoid;break-before: auto;"
    css["h1,h2,h3,h4"] = "break-inside: avoid;";
    css["h1::after,h2::after,h3::after,h4::after"] = "content: \"\";display: block;height: 200px;margin-bottom: -200px;";

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

    # Font Family:
    ff = "sans-serif";
    fs = "11pt";

    for(i = 0; i<=255; i++)_hex[sprintf("%02X",i)]=i;

    # Light theme colors:
    lt = "{--color: #263053; --alt-color: #16174c; --heading: #2A437E; --background: #FDFDFD; --alt-background: #F9FAFF;}";
    # Dark theme colors:
    dt = "{--color: #E9ECFF; --alt-color: #9DAFE6; --heading: #6C89E8; --background: #13192B; --alt-background: #232A42;}";

    ss = ss "\nbody " lt;
    ss = ss "\nbody.dark-theme " dt;
    ss = ss "\n@media (prefers-color-scheme: dark) {"
    ss = ss "\n  body " dt;
    ss = ss "\n  body.light-theme " lt;
    ss = ss "\n}"
    for(k in css)
        ss = ss "\n" k "{" css[k] "}";
    gsub(/%maxwidth%/,MaxWidth,ss);
    gsub(/%font-family%/,ff,ss);
    gsub(/%font-size%/,fs,ss);
    gsub(/%hr%/,hr,ss);

    return ss;
}
