for filepath in ./*.slx; do
  # set required variables
  #echo $filepath;
  fullfile=$(basename -- "$filepath");
  #echo $fullfile;
  filename="${fullfile%.*}";
  #echo $filename;
  extension="${fullfile##*.}";
  #echo $extension;
  buildfolder="$filename""_grt_rtw"
  #echo $buildfolder

  # rm build folders
  rm -rf $buildfolder
  rm -rf slprj

  # rm extra files
  if [ -f "${filename}.slxc" ]; then
    rm "${filename}.slxc"
  fi

  if [ -f "${filename}.mat" ]; then
    rm "${filename}.mat"
  fi

  # rm binary file
  if [ -f $filename ]; then
    rm $filename
  fi
done;

