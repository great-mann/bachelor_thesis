% Copyright (C) 2000 Robert Tolksdorf, Altvaterstr. 29, 14129 Berlin
% tolk@cs.tu-berlin.de
%
% dpunkt.cls
% Dokumentenklasse fuer LaTeX2e entsprechend dem dpunkt-Layout
% by Robert Tolksdorf (tolk@cs.tu-berlin.de)
%
\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{bart}[04/11/14 Copyright (C) 2015 by Thies Pfeiffer 1.1]

% 1.1
% 005: \raggedbottom in class, P{length} columntype corrected
%
% 001: psboxit required, nicer list of figures, nicer list of
% tables, stupid additional space with labels removed, forbid
% \item[xx]'s for itemize and enumerate
% todo:
% enter head for listoffigures/tables to toc??
% nested lists without par break
% latin1 als option?

\RequirePackage[dvips]{color}
\RequirePackage{pifont}
\RequirePackage{ifthen}
\RequirePackage{calc}
\RequirePackage{colortab}
\RequirePackage{array}
%\RequirePackage{psboxit}

%\RequirePackage{latin1}

\newboolean{DP@twoside}
\setboolean{DP@twoside}{true}

\newcount\DP@opt
\newcount\DP@fonts\DP@fonts=0

\newlength\magicA \magicA=2.2mm
\newlength\magicB \magicB=0mm
\newlength\magicC \magicC=1ex

\DeclareOption{oneside}{\setboolean{DP@twoside}{false}}
\DeclareOption{twoside}{\setboolean{DP@twoside}{true}}
\DeclareOption{cropmarks}{\DP@opt=1}
\DeclareOption{courier}{\DP@fonts=1}
\DeclareOption{ncs}{\DP@fonts=2}
\DeclareOption{robert}{%
  \magicA=2.15mm
  \magicB=-10pt%5pt%-14pt
  \magicC=3ex%3ex
}

\ProcessOptions

\ifcase\DP@opt
\or     \RequirePackage{cropmarks}
        \setlength{\cropwidth}{.2pt}
\fi

% Font changes: New Century Schoolbook and Courier
\ifcase\DP@fonts
\or     \renewcommand{\ttdefault}{pcr}
\or     \renewcommand{\rmdefault}{pnc}
        \renewcommand{\ttdefault}{pcr}
\fi

\newcounter{dpcnta}\newcounter{dpcntb}

\def\layoutpage{%
\hbox{\color[named]{Gray}%
 \makebox[0pt][r]{\raisebox{\textheight}[0pt][0pt]{\vbox to0pt{\sffamily
  \setcounter{dpcnta}{1}\setcounter{dpcntb}{(\textheight/\baselineskip)+2}
  \settowidth{\@tempdima}{\hbox{00}}
  \whiledo{\value{dpcnta}<\value{dpcntb}}{%
   \hbox to\@tempdima{\hfil\thedpcnta}\addtocounter{dpcnta}{1}
  }\vss}}}%
 \kern\textwidth\hbox to 0pt{\hss\vrule width.2pt height\textheight\hss}%
 \hskip\marginparsep\hbox to 0pt{\hss\vrule width.2pt height\textheight\hss}%
 \hskip\marginparwidth\hbox to 0pt{\hss\vrule width.2pt height\textheight\hss}}
 \newpage
}

\definecolor{dpunktgray}{gray}{.8}
%
% Hier kann man einseitige Ausgabe umsetzen: oneside
%
\ifthenelse{\boolean{DP@twoside}}
{\LoadClass[twoside]{book}}
{\LoadClass[oneside]{book}}

%\RequirePackage{german}

\newcount\DP@babel
\ifx\undefined\LTX@label
 \ifx\undefined\bbl@pageref     \DP@babel=0     % -> use \ref
 \else                          \DP@babel=2     % -> use \bbl@pageref
 \fi\else                       \DP@babel=1     % -> use \LTX@label
\fi

\def\DP@label#1{%
 \ifcase\DP@babel       \label{#1}%
 \or                    \LTX@label{#1}%
 \or                    \label{#1}%
 \fi}

\def\DP@pageref#1{
 \ifcase\DP@babel       \pageref{#1}%
 \or                    \LTX@pageref{#1}%
 \or                    \bbl@pageref{#1}%
 \fi}

% Header
\def\ps@dpunkt{%
  \let\@mkboth\markboth
  \def\@oddfoot{}\def\@evenfoot{}%       No feet.
  \def\@evenhead{%
    \kern-\originalevenmarginwidth%
    \hbox to0pt{%
      \vbox{%
        \hbox{%
          \sffamily\bfseries%
          \hbox to\originalevenmarginwidth{\invpagee\hfill}% %changed from odd to even
          \hbox to\originaltextwidth{\leftmark\strut\hfill}}%
        \hbox{\vrule width\totalwidth height.4pt}}%
      \hss}%
    \hss}
  \def\@oddhead{\hbox to0pt{%
    \vbox{%
      \hbox{%
        \sffamily\bfseries\hbox to\originaltextwidth{\hfill\strut\rightmark}%
        \hbox to\originaloddmarginwidth{\hfill\invpageo}}%
      \hbox{\vrule width\totalwidth height.4pt}}
    \hss}\hss}
  \def\chaptermark##1{%
    \markboth{\ifnum\c@secnumdepth >\m@ne\if@mainmatter\thechapter\ \fi\fi##1}%
             {}}%
  \def\sectionmark##1{%
    \markright{\ifnum\c@secnumdepth >\z@ \thesection \ \fi##1}}
}

% header without title but with line and number
\def\ps@dpunktplain{%
  \let\@mkboth\markboth
  \def\@oddfoot{}\def\@evenfoot{}%       No feet.
  \def\@evenhead{%
    \kern-\originalevenmarginwidth%
    \hbox to0pt{%
      \vbox{%
        \hbox{%
          \sffamily\bfseries%
          \hbox to\originalevenmarginwidth{\invpagee\hfill}% % changed from odd to even
          \hbox to\originaltextwidth{\strut\hfill}}%
        \hbox{\vrule width\totalwidth height.4pt}}%
      \hss}%
    \hss}
  \def\@oddhead{\hbox to0pt{%
    \vbox{%
      \hbox{%
        \sffamily\bfseries\hbox to\originaltextwidth{\hfill\strut}%
        \hbox to\originaloddmarginwidth{\hfill\invpageo}}%
      \hbox{\vrule width\totalwidth height.4pt}}
    \hss}\hss}
  \def\chaptermark##1{%
    \markboth{\ifnum\c@secnumdepth >\m@ne\if@mainmatter\thechapter\ \fi\fi##1}%
             {}}%
  \def\sectionmark##1{%
    \markright{\ifnum\c@secnumdepth >\z@ \thesection \ \fi##1}}
}

% new changed
\def\invpageo{{\setlength{\fboxsep}{\DP@fboxsep}%
  \vbox{%
    \hrule width8mm height1.5pt%
    \hbox to8mm{\hfill\vrule width1.5pt height1.5pt}%
    \hbox to8mm{\hfill\thepage\,\,\vrule width1.5pt}}}}
\def\invpagee{{\setlength{\fboxsep}{\DP@fboxsep}%
  \vbox{%
    \hrule width8mm height1.5pt%
    \hrule width1.5pt height1.5pt%
    \hbox to8mm{\vrule width1.5pt\,\,\thepage\hfill}}}}

\def\ps@plain{\ps@dpunkt}

\pagestyle{dpunkt}

% Sectioning

\def\preparehead{\parindent \z@ \raggedright \reset@font}

\def\@makechapterhead#1{%
  {\leavevmode\preparehead%
% this is ugly
   \hspace*{-.6cm}\ifnum \c@secnumdepth >\m@ne\if@mainmatter%
        \LARGE\sffamily\bfseries\hbox to1.1cm{\thechapter\hfil}%
    \fi\fi\hangindent1.1cm\LARGE\sffamily\bfseries #1\par\nobreak\vskip 36mm}}

\def\@makeschapterhead#1{%
  {\@mkboth{#1}{#1}%
   \preparehead
      \thispagestyle{dpunktplain}%
   \LARGE\sffamily\bfseries #1\nobreak\vskip 36mm}}


\def\Chapter*#1{%
  \chapter*{#1}
  \addcontentsline{toc}{chapter}{#1}
}

\def\@part[#1]#2{%
    \ifnum \c@secnumdepth >-2\relax
      \refstepcounter{part}%
      \addcontentsline{toc}{part}{\thepart\hspace{1em}#1}%
    \else
      \addcontentsline{toc}{part}{#1}%
    \fi
    \markboth{}{}%
    \thispagestyle{empty}
    {\centering
     \interlinepenalty \@M
     \normalfont
     \ifnum \c@secnumdepth >-2\relax
       \huge\bfseries\sffamily \partname~\thepart
       \par
       \vskip 20\p@
     \fi
     \Huge \bfseries\sffamily #2\par}%
    \@endpart}
\def\@spart#1{%
    {\centering
    \thispagestyle{empty}
     \interlinepenalty \@M
     \normalfont
     \Huge \bfseries\sffamily #1\par}%
    \@endpart}

\renewcommand{\section}{\@startsection {section}{1}{\z@}%
 {-3.5ex \@plus -1ex \@minus -.2ex}%
 {2.3ex \@plus.2ex}{\reset@font\Large\sffamily\bfseries}}
\renewcommand{\subsection}{\@startsection{subsection}{2}{\z@}%
 {-3.25ex\@plus -1ex \@minus -.2ex}%
 {1.5ex \@plus .2ex}{\reset@font\large\sffamily\bfseries}}
\renewcommand{\subsubsection}{\@startsection{subsubsection}{3}{\z@}%
 {-3.25ex\@plus -1ex \@minus -.2ex}%
 {1.5ex \@plus .2ex}{\reset@font\normalsize\sffamily\bfseries}}
\renewcommand{\paragraph}{\@startsection{paragraph}{4}{\z@}%
 {3.25ex \@plus1ex \@minus.2ex}{-1em}%
 {\reset@font\normalsize\sffamily\bfseries}}
\renewcommand{\subparagraph}{\@startsection{subparagraph}{5}{\parindent}%
 {3.25ex \@plus1ex \@minus .2ex}{-1em}%
 {\reset@font\normalsize\sffamily\bfseries}}

\def\cleardoublepage{\clearpage\if@twoside \ifodd\c@page\else
    \thispagestyle{empty}\hbox{}\newpage\if@twocolumn\hbox{}\newpage\fi\fi\fi}

\title{Noch ohne Titel\\(\texttt{\char92title} nicht benutzt)}
\author{Noch ohne Autoren\\(\texttt{\char92author} nicht benutzt)}

\newenvironment{thepreamble}%
{\maketitle\pagenumbering{roman}\setcounter{page}{5}}%
{\pagenumbering{arabic}\setcounter{page}{1}}

\renewenvironment{frontmatter}%
{\maketitle\pagenumbering{roman}\setcounter{page}{5}}%
{\pagenumbering{arabic}\setcounter{page}{1}}


\let\oldmaketitle\maketitle
\def\maketitle{%
  {\date{\today}\sffamily\Large\oldmaketitle}
  \cleardoublepage
}

% toc

\renewcommand\tableofcontents{%
    \if@twocolumn\@restonecoltrue\onecolumn\else\@restonecolfalse\fi
    {\sffamily\chapter*{\contentsname}
%        \@mkboth{\contentsname}{\contentsname}%
      \thispagestyle{dpunktplain}%
      \@starttoc{toc}%
      \if@restonecol\twocolumn\fi}%
    \cleardoublepage%
}

\renewcommand{\@dotsep}{2}

\renewcommand{\l@part}[2]{%
  \ifnum \c@tocdepth >-2\relax
    \addpenalty{-\@highpenalty}\addvspace{2.25em \@plus\p@}%
    \begingroup\setlength\@tempdima{3em}%
      \parindent \z@ \rightskip \@pnumwidth \parfillskip -\@pnumwidth
      {\leavevmode\large \bfseries #1\hfil \hbox to\@pnumwidth{\hss\sffamily #2}}\par
       \nobreak\global\@nobreaktrue\everypar{\global\@nobreakfalse\everypar{}}
    \endgroup
  \fi}

\newlength\subindent

% intercept \numberline to get width of last chapter number in toc
\def\mynumberline#1{%
  \settowidth{\DP@tempdim}{\hbox{#1.}}%
  \global\subindent\DP@tempdim%
  \oldnumberline{#1}%
}
\let\oldnumberline\numberline

\renewcommand{\l@chapter}[2]{%
 \ifnum \c@tocdepth >\m@ne
  \addpenalty{-\@highpenalty}\vskip 1.0em \@plus\p@\setlength\@tempdima{1.5em}%
   \begingroup
   \let\numberline\mynumberline
    \sffamily\bfseries\@dottedtocline{0}{0pt}{1.3cm}{#1}{{\sffamily\bfseries #2}}
    \penalty\@highpenalty
   \endgroup
   

 \fi}
\renewcommand{\l@section}[2]%
 {\@dottedtocline{1}{1.5em}{2.3em}{#1}{\sffamily #2}}
\renewcommand{\l@subsection}[2]%
 {\@dottedtocline{2}{3.8em}{3.2em}{#1}{\sffamily #2}}%\subindent
\renewcommand{\l@subsubsection}[2]%
 {\@dottedtocline{3}{7.0em}{4.1em}{#1}{\sffamily #2}}

%\renewcommand{\l@section}[2]%
% {\@dottedtocline{1}{0pt}{1.3cm}{#1}{\sffamily #2}}
%\renewcommand{\l@subsection}[2]%
% {\@dottedtocline{2}{\subindent}{1.3cm}{#1}{\sffamily #2}}%\subindent
%\renewcommand{\l@subsubsection}[2]%
% {\@dottedtocline{3}{2\subindent}{1.3cm}{#1}{\sffamily #2}}
\renewcommand{\l@paragraph}[2]%
 {\@dottedtocline{4}{0pt}{1.5cm}{#1}{\sffamily #2}}
\renewcommand{\l@subparagraph}[2]%
 {\@dottedtocline{5}{0pt}{1.5cm}{#1}{\sffamily #2}}

% list of figures and list of tables nice
\renewcommand\listoffigures{%
    \if@twocolumn\@restonecoltrue\onecolumn\else\@restonecolfalse\fi
    {\sffamily\Chapter*{\listfigurename}%
      \@mkboth{\listfigurename}{\listfigurename}%
    \@starttoc{lof}%
    \if@restonecol\twocolumn\fi
    }\cleardoublepage}

\renewcommand\listoftables{%
    \if@twocolumn\@restonecoltrue\onecolumn\else\@restonecolfalse\fi
    {\sffamily\Chapter*{\listtablename}%
      \@mkboth{\listtablename}{\listtablename}%
    \@starttoc{lot}%
    \if@restonecol\twocolumn\fi
    }\cleardoublepage}

\renewcommand{\l@figure}[2]%
 {\@dottedtocline{1}{0pt}{1.1cm}{#1}{\sffamily #2}}
\renewcommand{\l@table}[2]%
 {\@dottedtocline{1}{0pt}{1.1cm}{#1}{\sffamily #2}}


% index

\usepackage{makeidx}
\makeindex

\def\indexintro{}
\def\indexname{Index}

\renewenvironment{theindex}
 {\cleardoublepage\addcontentsline{toc}{chapter}{\indexname}%
  \if@twocolumn\@restonecolfalse\else\@restonecoltrue\fi
  \columnseprule \z@ \columnsep 35\p@
  \twocolumn[\@makeschapterhead{\indexname}\indexintro]\@mkboth{}{}%
  \thispagestyle{dpunktplain}\parindent\z@ \parskip\z@ \@plus .3\p@\relax
  \@mkboth{{\indexname}}{{\indexname}}
  \let\item\@idxitem\bgroup\sffamily\fontsize{8}{14pt}\selectfont\raggedright\sloppy}%
 {\egroup\if@restonecol\onecolumn\else\clearpage\fi}

\renewcommand{\seealso}[2]{{\em \alsoname\/} #1\\}

% footnotes
\let\footnotesize\small
\renewcommand{\footnoterule}{\kern-3\p@  \hrule width 2cm \kern 2.6\p@}

\let\oldfootnote\footnote
\def\footnote{%
 \@ifstar{\oldfootnote}%
 {\ifx\@footnotetext\@mpfootnotetext
  \let\DP@next\oldfootnote
 \else
  \let\DP@next\DP@footnote
 \fi\DP@next}}

\let\oldtexttt\texttt
\def\nonitalictt#1{{\upshape\oldtexttt{#1}}}

\def\setfootnote#1{%
 \sffamily\itshape{\let\texttt\nonitalictt\footnotesize#1}}

\def\DP@footnote#1{%
 \marginpar[\raggedleft\setfootnote{#1}]%
           {\raggedright\setfootnote{#1}}%
}

\long\def\@ympar#1{%
  \@savemarbox\@marbox{\raggedright\setfootnote{#1}}%
  \global\setbox\@currbox\copy\@marbox
  \@xympar}

\long\def\@xmpar[#1]#2{%
  \@savemarbox\@marbox{\raggedleft\setfootnote{#1}}%
  \@savemarbox\@currbox{\raggedright\setfootnote{#2}}%
  \@xympar}

% figures and captions
\newbox\DP@figbox
\newlength{\DP@figwidth}
\newcounter{DP@captlab}
\setcounter{DP@captlab}{1}

\newdimen\@captionkern

\def\@captionaside#1#2{%
 \ifthenelse{\boolean{DP@twoside} \and \not \isodd{\DP@pageref{DP@lcaptlab\roman{DP@captlab}}}}%
 {\let\@captionplace=\raggedleft%
   \setlength{\@captionkern}{\magicA-\marginwidth-\marginparsep}}%
 {\let\@captionplace=\raggedright%
   \setlength{\@captionkern}{\textwidth+\marginparsep-\magicA}}
 \@putcaption{#1}{#2}%
 \DP@label{DP@lcaptlab\roman{DP@captlab}}%
 \stepcounter{DP@captlab}}

\def\@putcaption#1#2{%
 \leavevmode%
\kern\@captionkern%
  \hbox to0pt{\vbox to0pt{%
%\vskip1ex%
      {\def\arraystretch{1}\begin{tabular}{p{\marginparwidth}}%
        \@captionplace\@captionruletop\\%
        \@captionplace\renewcommand{\baselinestretch}{1.08}\sffamily\itshape\small{\bfseries
          #1}\\\let\texttt\nonitalictt#2\\[-0.5\baselineskip] % Blank important!
        \@captionrulebottom%
        \end{tabular}}\vss}\hss}}

\def\@captionruletop{%
  {\marginparwidth=.985\marginparwidth%
  {\fboxsep=.25\fboxsep%
    \colorbox{dpunktgray}{%
      \hbox to\marginparwidth{\hss%
%       \hbox{%
%         \vbox{%
%           \hrule depth.5\fboxsep width\marginparwidth
%           height.5\fboxsep%
%           }%
         }%
       }%
    }%
  }
}

\def\@captionrulebottom{\@captionruletop}

\def\@captionbody#1#2{%
 \@captionruletop{}%   
 \renewcommand{\baselinestretch}{1.08}%1.08
 \sffamily\itshape\small{\bfseries #1}\\#2 % Blank important!
 \@captionrulebottom%
}

\renewcommand{\@makecaption}[2]{\@captionaside{#1}{#2}}

\newcounter{DP@figlab}\setcounter{DP@figlab}{1}
\newcounter{DP@tmpcnt}
\newcount\DP@tempcnt
\newdimen\DP@tempdim

\def\widefloatstart{%
  \egroup%
  \settoheight{\DP@tempdim}{\parbox[t]{\marginparwidth}%
                     {\raggedright\sffamily\itshape{\bfseries
                         a}\\\figcapt}}%
  \@tempdima=\DP@tempdim%
  \advance\@tempdima by\ht\DP@figbox%
%  \@ifundefined{r@DP@lfiglab\roman{DP@figlab}}%
%    {\DP@tempcnt=0}%
%    {\DP@tempcnt=\DP@pageref{DP@lfiglab\roman{DP@figlab}}}%
  \setcounter{DP@tmpcnt}{\DP@tempcnt}%
  \bgroup\samepage\ifthenelse{\isodd{\value{DP@tmpcnt}}}%
   {\hbox to0pt{\box\DP@figbox\hss}}% odd
   {\DP@figwidth=\wd\DP@figbox% even
     \advance\DP@figwidth by-\textwidth%
     \advance\DP@figwidth by-\marginparsep%
     \advance\DP@figwidth by-\magicB%
    \leavevmode\kern-\DP@figwidth\hbox to0pt{\box\DP@figbox\hss}}%
  \vspace*{-\magicC}\caption{\DP@label{\figlab}\figcapt}\egroup%
%  \DP@label{DP@lfiglab\roman{DP@figlab}}\stepcounter{DP@figlab}%
  \advance\DP@tempdim by\baselineskip\vspace*{\DP@tempdim}%
}



% tab{label}{caption}
\newenvironment{tab}[2]%
 {\begin{table}[htb]\bgroup\samepage\caption{\label{#1}#2}\begin{center}}%
 {\end{center}\egroup\end{table}}

% tabhere{label}{caption}
\newenvironment{tabhere}[2]%
 {\begin{table}[H]\bgroup\samepage\caption{\label{#1}#2}\begin{center}}%
 {\end{center}\egroup\end{table}}

% widetab{label}{caption}
\newenvironment{widetab}[2]{%
  \begin{table}[htb]\def\figlab{#1}\def\figcapt{#2}%
  \setbox\DP@figbox=\hbox\bgroup\ignorespaces}%
 {\widefloatstart\end{table}}

\newenvironment{widetabhere}[2]{%
  \begin{table}[H]\def\figlab{#1}\def\figcapt{#2}%
  \setbox\DP@figbox=\hbox\bgroup\ignorespaces}%
 {\widefloatstart\end{table}}

\def\figurestart#1#2{%
  \bgroup\samepage\caption{\label{#1}#2}\begin{center}\penalty10000}
\def\figureend#1#2{%
  \end{center}\egroup\end{figure}}


% fig{label}{caption}
\newenvironment{fig}[2]%
 {\begin{figure}[htb]\figurestart{#1}{#2}}%
 {\figureend}

\newenvironment{figtb}[2]%
 {\begin{figure}[tb]\figurestart{#1}{#2}}%
 {\figureend}

% fighere{label}{caption}
\newenvironment{fighere}[2]%
 {\intextsep10pt\begin{figure}[H]\figurestart{#1}{#2}}%
 {\figureend}

% widefig{label}{caption}
\newenvironment{widefig}[2]{%
  \begin{figure}[htb]\def\figlab{#1}\def\figcapt{#2}%
  \setbox\DP@figbox=\hbox\bgroup\ignorespaces}%
 {\widefloatstart\end{figure}}

\newenvironment{widefighere}[2]{%
  \begin{figure}[H]\def\figlab{#1}\def\figcapt{#2}%
  \setbox\DP@figbox=\hbox\bgroup\ignorespaces}%
 {\widefloatstart\end{figure}}

% wide
\newenvironment{wide}{%
  \setbox\DP@figbox=\hbox\bgroup\ignorespaces}%
 {\egroup%
  \@ifundefined{r@DP@lfiglab\roman{DP@figlab}}%
   {\DP@tempcnt=0}{\DP@tempcnt=\DP@pageref{DP@lfiglab\roman{DP@figlab}}}%
  \setcounter{DP@tmpcnt}{\DP@tempcnt}%
  \ifthenelse{\isodd{\value{DP@tmpcnt}}}%
   {\hbox to0pt{\box\DP@figbox\hss}}%
   {\DP@figwidth=\wd\DP@figbox\advance\DP@figwidth by-\textwidth%
     \advance\DP@figwidth by-\magicB%
    \leavevmode\kern-\DP@figwidth\hbox to0pt{\box\DP@figbox\hss}}%
  \DP@label{DP@lfiglab\roman{DP@figlab}}\stepcounter{DP@figlab}}

% \meta{#1}: Some meta-code in tt-italics with hyphenation
% \meta*{#1}: Some meta-code in tt-italics without hyphenation
% \code{#1}: Some code in tt without hyphenation
% \code*{#1}: Some code in tt with hyphenation
\def\meta{\@ifnextchar *{\DP@metas}{\DP@meta}}
\def\DP@metas*#1{{\ttfamily\itshape #1}}
\def\DP@meta#1{{\ttfamily\itshape\hyphenchar\font=\defaulthyphenchar #1}}
\def\code{\@ifnextchar *{\DP@codes}{\DP@code}}
\def\DP@codes*#1{{\ttfamily\hyphenchar\font=\defaulthyphenchar #1}}
\def\DP@code#1{{\ttfamily #1}}
\def\type{\@ifnextchar *{\DP@types}{\DP@type}}
\def\DP@types*#1{{\ttfamily\hyphenchar\font=\defaulthyphenchar #1}}
\def\DP@type#1{{\ttfamily #1}}
\def\var{\@ifnextchar *{\DP@vars}{\DP@var}}
\def\DP@vars*#1{{\ttfamily\hyphenchar\font=\defaulthyphenchar #1}}
\def\DP@var#1{{\ttfamily\bf #1}}
\def\datei#1{{\tt #1}}
\def\programm#1{\textsf{\textsl{#1}}}

% lists
\setlength{\listparindent}{0pt}
\setlength{\itemindent}{0pt}
\setlength{\leftmargin}{1.1cm}
\setlength{\labelwidth}{1.1cm}
\newlength{\oldlabelsep}
\setlength{\oldlabelsep}{\labelsep}
\setlength{\labelsep}{0pt}

\def\itemlabelchar#1{%
        \def\@itemlabelchar{#1}}
\itemlabelchar{\ding{111}}

\newlength{\itemlabelcharwidth}
\settowidth{\itemlabelcharwidth}{\small\@itemlabelchar}

\def\@setitemlabelchar{%
  \hbox to\itemlabelcharwidth{\hss\small\@itemlabelchar\hss}}


\renewcommand{\labelitemi}%
  {\hbox to 1.1cm{\hskip.6cm{\@setitemlabelchar}\hfill}}
\renewcommand{\labelitemii}%
  {\hbox to 1.1cm{\hskip.6cm{\@setitemlabelchar}\hfill}}
%todo ab 10 komisch
\renewcommand{\labelenumi}%
  {\hbox to 1.1cm{\hskip.6cm\theenumi.\hfill}}

\renewenvironment{description}
               {\setlength{\labelsep}{\oldlabelsep}%
                 \list{}{\labelwidth\z@ \itemindent-\leftmargin
                        \let\makelabel\descriptionlabel}}
               {\endlist}

\renewenvironment{quote}
 {\list{}{\leftmargin.6cm\rightmargin\leftmargin}\item[]}{\endlist}
\renewenvironment{quotation}
 {\list{}{\listparindent 1.5em\leftmargin.6cm\itemindent\listparindent%
          \rightmargin\leftmargin\parsep\z@ \@plus\p@}\item[]}{\endlist}

% after every list no indent!
\def\@doendpe{\@endpetrue
     \def\par{\@restorepar\everypar{}\par\@endpefalse\noindent}%
     \everypar{{\setbox\z@\lastbox}\everypar{}\@endpefalse}%
    }

% Page dimensions
\setlength{\parindent}{6mm}
\setlength{\textwidth}{12.7cm}\setlength{\textheight}{19.5cm} %HIER TEXTWIDTH AENDERN
\newlength{\originaltextwidth}\setlength{\originaltextwidth}{\textwidth}
\setlength{\marginparsep}{0.5cm}\setlength{\marginparwidth}{2.5cm} %UND HIER
\newlength{\marginwidth}\setlength{\marginwidth}{\marginparsep+\marginparwidth}
\newlength{\originalevenmarginwidth}\setlength{\originalevenmarginwidth}{\marginwidth}
%\addtolength{\originalevenmarginwidth}{-0.7cm}
\newlength{\originaloddmarginwidth}\setlength{\originaloddmarginwidth}{\marginwidth}
\newlength{\totalwidth}\setlength{\totalwidth}{\textwidth+\marginwidth}
% substract 14pt to avoid strange overfull boxes. reason not clear
\newlength{\plussepwidth}\setlength{\plussepwidth}{\textwidth+\marginparsep}%-14pt}

% Save \fboxsep
\newlength{\DP@fboxsep}\setlength{\DP@fboxsep}{\fboxsep}
% Head dimensions
\setlength{\headsep}{0.675cm}
\settoheight{\headheight}{\invpageo}
\addtolength{\headheight}{\fboxrule+\fboxsep}
\addtolength{\headheight}{.70004pt}

% linespacing
\renewcommand{\baselinestretch}{1.08} %UND HIER ZEILENABSTAND 1.08

% quotes are french!
%\def\glqq{{\fontencoding{T1}\fontfamily{pnc}\selectfont\char20}}
%\def\grqq{{\fontencoding{T1}\fontfamily{pnc}\selectfont\char19}}
%\def\glq{{\fontencoding{T1}\fontfamily{pnc}\selectfont\char16}}
%\def\grq{{\fontencoding{T1}\fontfamily{pnc}\selectfont\char15}}
%\ifx\undefined\quotedblbase\else
% \renewcommand{\quotedblbase}{\glqq}
% \renewcommand{\textquotedblleft}{\grqq}
%\fi

% bibliography

\renewenvironment{thebibliography}[1]
     {\chapter*{\bibname}
        \@mkboth{{\bibname}}{{\bibname}}%
      \addcontentsline{toc}{chapter}{\bibname}%  
      \list{\@biblabel{\arabic{enumiv}}}%
           {\settowidth\labelwidth{\@biblabel{#1~~}}%
            \leftmargin\labelwidth
            \advance\leftmargin\labelsep
%            \if@openbib
%              \advance\leftmargin\bibindent
%              \itemindent -\bibindent
%              \listparindent \itemindent
%              \parsep \z@
%            \fi
            \usecounter{enumiv}%
            \let\p@enumiv\@empty
            \renewcommand{\theenumiv}{\arabic{enumiv}}}%
%      \if@openbib
%        \renewcommand{\newblock}{\par}
%      \else
        \renewcommand{\newblock}{\hskip .11em \@plus.33em \@minus.07em}%
%      \fi
      \sloppy\clubpenalty4000\widowpenalty4000%
      \raggedright
      \sfcode`\.=\@m}
     {\def\@noitemerr
       {\@latex@warning{Empty `thebibliography' environment}}%
      \endlist}

% figure-placement stuff: be aesthetically rude and allow very much
% figures along with very few text.
\setcounter{topnumber}{5}\setcounter{bottomnumber}{5}\clubpenalty=10000
\setcounter{totalnumber}{10}\renewcommand{\topfraction}{1}
\renewcommand{\topfraction}{1}\renewcommand{\bottomfraction}{1}
\renewcommand{\textfraction}{0}\widowpenalty=10000


\newif\iftabularheader\tabularheaderfalse
\def\preserveBackSlash#1{\let\temp=\\#1\let\\=\temp}
\def\tabularcellfontstuff{\preserveBackSlash\raggedright%
    \fontsize{8.5}{10.5pt}\selectfont}
\def\tabularheadfontstuff{\sf\bfseries{\Large\strut}}
\def\tabularstylesf{%
  \sf\tabularcellfontstuff
  \iftabularheader\tabularheadfontstuff\fi}

\def\tabularstylett{
  \iftabularheader\tabularcellfontstuff\tabularheadfontstuff%
  \else\tt\tabularcellfontstuff
  \fi}

\newcolumntype{R}{>{\bgroup\tabularstylesf}r<{\egroup}}
\newcolumntype{Y}{>{\bgroup\tabularstylesf}X<{\egroup}}
\newcolumntype{P}[1]{>{\bgroup\tabularstylesf}p{#1}<{\egroup}}
\newcolumntype{L}{>{\bgroup\tabularstylesf}l<{\egroup}}
\newcolumntype{C}{>{\bgroup\tabularstylesf}c<{\egroup}}
\newcolumntype{T}{>{\bgroup\tabularstylett}l<{\egroup}}
\newcolumntype{t}{>{\bgroup\tabularstylett}r<{\egroup}}

%\extrarowheight=2pt
\def\arraystretch{1.2}

\def\tabularheader#1{\sf\bfseries{\Large\strut}#1}

\def\tabulargray{\special{ps: gsave .87 setgray}\aftergroup\restoregray}
\def\tabulardarkgray{\special{ps: gsave .50 setgray}\aftergroup\restoregray}
\def\restoregray{\special{ps: grestore}}

\def\tabularlightgray{\special{ps: gsave .95 setgray}\aftergroup\restorelightgray}
\def\restorelightgray{\special{ps: grestore}}

\def\tabularhead#1\\#2\endtabularhead{\CC@{\@preamble}{#1}{#2}}

\newlength\tabbottomskip
\tabbottomskip=\smallskipamount
\newlength\tabtopskip
\tabtopskip=\tabbottomskip
\advance\tabtopskip by-\baselineskip
\def\topseparation{\\[\tabtopskip]}
\def\bottomseparationline{\\[\tabbottomskip]\hline}

%bib abstand zum label.

\def\@bibitem#1{\item\if@filesw \immediate\write\@auxout
       {\string\bibcite{#1}{\the\value{\@listctr}}}\fi%
       \hskip.5em plus0pt minus0pt\ignorespaces}

\endinput

% widerfig{label}{caption}
\newenvironment{widerfig}[2]{%
  \begin{figure}[htb]\def\figlab{#1}\def\figcapt{#2}%
  \setbox\DP@figbox=\hbox\bgroup\ignorespaces}%
 {\egroup\settoheight{\DP@tempdim}{\parbox[t]{\marginparwidth}%
                     {\raggedright\sffamily\itshape{\bfseries a}\\\figcapt}}%
  \@tempdima=\DP@tempdim\advance\@tempdima by\ht\DP@figbox%
  \@ifundefined{r@DP@lfiglab\roman{DP@figlab}}%
   {\DP@tempcnt=0}{\DP@tempcnt=\DP@pageref{DP@lfiglab\roman{DP@figlab}}}%
  \setcounter{DP@tmpcnt}{\DP@tempcnt}%
  \bgroup\samepage\ifthenelse{\isodd{\value{DP@tmpcnt}}}%
   {\hbox to0pt{\box\DP@figbox\hss}}%
   {\DP@figwidth=\wd\DP@figbox\advance\DP@figwidth by-\textwidth%
    \leavevmode\kern-\DP@figwidth\hbox to0pt{\box\DP@figbox\hss}}%
  \caption{\protect\DP@label{\figlab}\figcapt}\egroup%
  \DP@label{DP@lfiglab\roman{DP@figlab}}\stepcounter{DP@figlab}%
  \advance\DP@tempdim by\baselineskip\vspace*{\DP@tempdim}\end{figure}}

\def\@putcaption#1#2{%
 \leavevmode%
\kern\@captionkern%
  \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
    {\@captionplace\@captionbody{#1}{#2}}\vss}\hss}}

%% \def\@captionaside#1#2{%
%%  \ifthenelse{\isodd{\DP@pageref{DP@lcaptlab\roman{DP@captlab}}}}%
%%  {\leavevmode\kern\plussepwidth%
%%   \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
%%     {\raggedright\sffamily\itshape\small\strut\\{\bfseries #1}\\#2}\vss}\hss}}%
%%  {\leavevmode\kern-\marginwidth%
%%   \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
%%     {\raggedleft\sffamily\itshape\small\strut\\{\bfseries #1}\\#2}\vss}\hss}}%
%%  \DP@label{DP@lcaptlab\roman{DP@captlab}}%
%%  \stepcounter{DP@captlab}}

\def\@captionaside#1#2{%
 \ifthenelse{\isodd{\DP@pageref{DP@lcaptlab\roman{DP@captlab}}}}%
 {\leavevmode\kern\plussepwidth%
  \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
    {\raggedright\@captionbody{#1}{#2}}\vss}\hss}}%
 {\leavevmode\kern-\marginwidth%
  \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
    {\raggedleft\@captionbody{#1}{#2}}\vss}\hss}}%
 \DP@label{DP@lcaptlab\roman{DP@captlab}}%
 \stepcounter{DP@captlab}}

\def\@captionbelow#1#2{%
 \DP@label{DP@lcaptlab\roman{DP@captlab}}%
 \ifthenelse{\isodd{\DP@pageref{DP@lcaptlab\roman{DP@captlab}}}}%
 {\leavevmode\kern\plussepwidth%
  \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
       {\raggedright\sffamily\itshape\small{\bfseries #1}\\#2(A)}\vss}\hss}}%
 {\leavevmode\kern-\marginwidth%
  \hbox to0pt{\vbox to0pt{\parbox[t]{\marginparwidth}%
       {\raggedleft\sffamily\itshape\small{\bfseries #1}\\#2(b)}\vss}\hss}}%
 \stepcounter{DP@captlab}
}

\raggedbottom

