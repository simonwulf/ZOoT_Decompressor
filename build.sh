if [ -e build -a ! -d build ]; then
  echo "ERROR: ./build exists but is not a directory"
  exit 1
elif [ ! -d build ]; then
  mkdir build
fi

OUT=build/ZOoT_Decompressor

g++ -w -o $OUT CRC.cpp Yaz0.cpp main.cpp

if [ $? ]; then
  echo "Successfully built $OUT"
fi
