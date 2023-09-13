# TODO: make this script take as input the .slx to build

if [ $# -ne 1 ]; then
  echo "Usage: ./bulid processModle.slx"
  exit
fi

fullfile=$(basename -- "$1");
echo $fullfile

filename="${fullfile%.*}";
echo $filename;

extension="${fullfile##*.}";
echo $extension;

buildfolder="${filename}_grt_rtw"
echo $buildfolder


if [ $extension != "slx" ]; then
  echo "Must supply a .slx file"
  exit
fi

# FYI: the model name is case sensitive
modelname=$filename
echo $modelname

apifile="${buildfolder}/${modelname}_capi.c"
echo $apifile

command -v matlab 2>&1 > /dev/null;
if [ $? -eq 0 ]; then
  echo "matlab is installed, proceeding to build Simulink process model..."
  # TODO: Research if there is the ability to hook the code from the command line
  matlab -nojvm -nodisplay -nodesktop -r "warning off; rtwbuild('${modelname}'); quit();"
else
  echo "matlab not installed, skipping Simulink process model build..."
fi

# mv inputs.txt inputs.bak
# mv outputs.txt outputs.bak
# 
# # TODO: Make sure this still works with other solvers
# cat $apifile \
#  | cut -d\" -f2 \
#  | cut -d\/ -f3 \
#  | cut -d" " -f2 \
#  | grep -Ev "^(sim)" \
#  | grep -E "(_AI|_DI)" \
#  | sort \
#  | uniq \
#  >> inputs.txt \
#  ;
# 
# cat $apifile \
#  | cut -d\" -f2 \
#  | cut -d\/ -f3 \
#  | grep -Ev "(sim|Scale)" \
#  | grep -E "(_AO|_DO)" \
#  | sort \
#  | uniq \
#  >> outputs.txt \
#  ;
# 
# cat $apifile \
#  | cut -d\" -f2 \
#  | cut -d\/ -f3 \
#  | cut -d" " -f2 \
#  | grep -Ev "^(sim)" \
#  | grep -E "(_PANEL)" \
#  | sort \
#  | uniq \
#  >> outputs.txt \
#  ;
