%\documentclass{standalone}
\documentclass{minimal}
\usepackage[rgb]{xcolor} % needed for color definitions in HSB
\usepackage{tikz}
\usepackage{fp}
\usepackage{ifthen}
\usetikzlibrary{arrows}
\usetikzlibrary{patterns}
%\usepackage{standalone}


\pgfdeclarelayer{background}
{{layerList}}
\pgfsetlayers{background,{{layerString}}main} % edit layer order (first is lowest; image on background)

\def\rangeHSB{360}
\def\picWidth{{{picWidth}}}
\def\picHeight{{{picHeight}}}
\newboolean{bDrawVectorBlock}

% Set global parameters here (linestyles, colors, opacities)
% \newcommand{\TikZPicWidth}{\textwidth} % Set width of the final picture (e.g. '0.8\textwidth' or '10cm'), only works if resizebox is used
\def\scaleFac{0.2} % scale factor of the tikz picture
  % Cropping options; Extend these borders for also displaying vectors going beyond image boundaries
  \def\xStartClip{0}
  \def\yStartClip{0}
  \def\xEndClip{{{picWidth}}}
  \def\yEndClip{{{picHeight}}}
    % origin at bottom left corner -> flip y coordinates
    \FPset\temp\yStartClip
    \FPsub\yStartClip\picHeight\yEndClip
    \FPsub\yEndClip\picHeight\temp
    % Adjust some parameters to scaling factor
    \FPmul\xStartClip\xStartClip\scaleFac
    \FPmul\yStartClip\yStartClip\scaleFac
    \FPmul\xEndClip\xEndClip\scaleFac
    \FPmul\yEndClip\yEndClip\scaleFac
    \FPmul\figWidth\picWidth\scaleFac
    \FPmul\figHeight\picHeight\scaleFac
% Parameters concerning layout of blocks
  \tikzstyle{blkLine}=[thin] % line style of blocks
  \tikzstyle{gridOpacity}=[draw opacity=1*#1] % Set opacity of the grid
  \tikzstyle{fillOpacity}=[fill opacity=0.5*#1] % Modify fill opacity of the blocks on a global level
  \tikzstyle{fillStyle}=[fill=#1,draw=#1] % Format grid color here (draw=<gridColor>; draw=#1 leads to color of the corresponding block)
% Parameters concerning layout of vectors (and vector-blocks)
  \newcommand{\circleRad}{0.5 pt} % radius of the circles used for (0,0) motion vectors
  \setboolean{bDrawVectorBlock}{false} % also draws a block for vector types if true
  \tikzstyle{dotLine}=[ultra thin] % line width of the circle used for (0,0) motion vectors
  \tikzstyle{dotStyle}=[fill=#1,draw=#1] % Remove fill command to get a circle instead of a dot
  \tikzstyle{vecBlkLine}=[thin] % line style of vector blocks
  \tikzstyle{vecOpacity}=[draw opacity=1*#1] % Modify opacity of vectors on a global level
  \tikzstyle{vecLine}=[very thin,>=stealth',->] % arrow style/thickness of vectors
  
\begin{document}
\begin{tikzpicture}
 
\begin{pgfonlayer}{background}
  \clip (\xStartClip pt,\yStartClip pt) rectangle (\xEndClip pt,\yEndClip pt);
  \node[anchor=south west,inner sep=0,fill=white,minimum width=\figWidth pt,minimum height=\figHeight pt] (image) at (0,0) {}; % include white rectangle as placeholder for image
  {{image}}
\end{pgfonlayer}
 
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Change type specific colors/opacities in this part

{{tikzranges}}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Macros for calculating colors/opacities for individual blocks

{{macroscalc}}
  
{{tikzpicture}}


\end{tikzpicture}  
\end{document}
  
