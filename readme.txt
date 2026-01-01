1->open folder in terminal
2->go to source 
3->go to cli
4->run this command :
g++ -std=c++14 -o main \
    main.cpp \
    -I/usr/include/opencv4 \
    -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_videoio -lopencv_objdetect -lopencv_dnn \
    -lpthread -lsqlite3 -ljsoncpp -ldlib \
    -llapack -lblas -lcrypto \
    -lleptonica -ltesseract

NOTE : make sure opencv and other required folder are installed
NOTE : modules folder is not implemented properly and has logical issues and incomplete implementation
       front is not implementation yet for now i am using cli for currect state of project .
       and computer vision is yet to be implemented
