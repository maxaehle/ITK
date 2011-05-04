#include <iostream>
#include <fstream>

#include "itkFileListVideoIO.h"
#include "itkImportImageFilter.h"
#include "itkRGBPixel.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"


// ITK typedefs
typedef itk::RGBPixel<char> PixelType;
typedef itk::ImportImageFilter<PixelType, 2> ImportFilterType;
typedef itk::Image<PixelType, 2> ImageType;
typedef itk::ImageFileReader<ImageType> ReaderType;
typedef itk::ImageFileWriter<ImageType> WriterType;


//
// Duplicate the splitting function from FileListVideoIO
//
std::vector<std::string> test_SplitFileNames(const char* fileList)
{
  std::string str = fileList;

  std::vector<std::string> out;

  int pos = 0;
  int len = str.length();
  while (pos != -1 && len > 0)
    {
    // Get the substring
    str = str.substr(pos, len);

    // Update pos
    pos = str.find(',');

    // Add the filename to the list
    out.push_back(str.substr(0,pos));

    // Move past the delimiter
    if (pos != -1)
      {
      pos++;
      }
    len -= pos;
    }

  return out;
}


///////////////////////////////////////////////////////////////////////////////
// This tests all of the functionality of the FileListVideoIO
//
// Usage: [Video Input] [Non-Video Input] [Video Output] [Width] [Height]
//            [Num Frames] [FpS]

int test_FileListVideoIO ( char* input, char* nonVideoInput, char* output, char* cameraOutput,
                         unsigned int inWidth, unsigned int inHeight, unsigned long inNumFrames,
                         double inFpS )
{

  int ret = EXIT_SUCCESS;

  // Create the VideoIO
  itk::FileListVideoIO::Pointer fileListIO = itk::FileListVideoIO::New();


  //////
  // SetFileName
  //////

  fileListIO->SetFileName(input);


  //////
  // CanReadFile
  //////
  std::cout << "FileListVideoIO::CanReadFile..." << std::endl;


  // Test CanReadFile on good file
  if (!fileListIO->CanReadFile(input))
    {
    std::cerr << "Could not read " << input << std::endl;
    ret = EXIT_FAILURE;
    }

  // Test CanReadFile on non-existant file
  std::string nonExistantFile = "Bad/Path/To/Nothing";
  if (fileListIO->CanReadFile(nonExistantFile.c_str()))
    {
    std::cerr << "Should have failed to open \"" << nonExistantFile << "\"" << std::endl;
    ret = EXIT_FAILURE; 
    }

  // Test CanReadFile on non-image file list
  if (fileListIO->CanReadFile(nonVideoInput))
    {
    std::cerr << "Should have failed to open \"" << nonVideoInput << "\"" << std::endl;
    ret = EXIT_FAILURE; 
    }


  //////
  // ReadImageInformation
  //////
  std::cout << "FileListVideoIO::ReadImageInformation..." << std::endl;

  fileListIO->SetFileName(input);
  fileListIO->ReadImageInformation();
  bool infoSet = true;
  std::stringstream paramMessage;
  if (fileListIO->GetDimensions(0) != inWidth)
    {
    infoSet = false;
    paramMessage << "Width mismatch: (expected) " << inWidth << " != (got) "
      << fileListIO->GetDimensions(0) << std::endl;
    }
  if (fileListIO->GetDimensions(1) != inHeight)
    {
    infoSet = false;
    paramMessage << "Height mismatch: (expected) " << inHeight << " != (got) "
      << fileListIO->GetDimensions(1) << std::endl;
    }
  double epsilon = 0.0001;
  if (fileListIO->GetFpS() < inFpS - epsilon || fileListIO->GetFpS() > inFpS + epsilon)
    {
    infoSet = false;
    paramMessage << "FpS mismatch: (expected) " << inFpS << " != (got) " << fileListIO->GetFpS()
      << std::endl;
    }
  if (fileListIO->GetFrameTotal() != inNumFrames)
    {
    infoSet = false;
    paramMessage << "FrameTotal mismatch: (expected) " << inNumFrames << " != (got) "
      << fileListIO->GetFrameTotal() << std::endl;
    }

  if (!infoSet)
    {
    std::cerr << paramMessage.str();
    ret = EXIT_FAILURE;
    }


  //////
  // Read
  //////
  std::cout << "FileListVideoIO::Read..." << std::endl;
  std::cout << "Comparing all " << fileListIO->GetFrameTotal() << " frames" << std::endl;

  // Set up ImageFileReader
  ReaderType::Pointer reader = ReaderType::New();

  // Loop through all frames
  std::vector<std::string> filenames = test_SplitFileNames(input);
  for (unsigned long i = 0; i < fileListIO->GetFrameTotal(); ++i)
    {
    // Read the image file directly
    reader->SetFileName(filenames[i]);
    reader->Update();

    // Read the image using FileListVideoIO
    size_t bufferSize = fileListIO->GetImageSizeInBytes();
    PixelType buffer[bufferSize];
    fileListIO->Read(static_cast<void*>(buffer));

    // Compare buffer contents
    if (memcmp(reinterpret_cast<void*>(buffer),
        reinterpret_cast<void*>(reader->GetOutput()->GetBufferPointer()), bufferSize))
      {
      std::cerr << "Frame buffers don't match for frame " << i << std::endl;
      ret = false;
      }
    }


  //////
  // SetNextFrameToRead
  //////
  std::cout << "FileListVideoIO::SetNextFrameToRead" << std::endl;

  // try seeking to the end
  unsigned long seekFrame = fileListIO->GetFrameTotal()-1;
  if (!fileListIO->SetNextFrameToRead(seekFrame))
    {
    std::cerr << "Failed to seek to second I-Frame..." << std::endl;
    ret = EXIT_FAILURE;
    }

  // Save the current parameters
  double fps = fileListIO->GetFpS();
  unsigned int width = fileListIO->GetDimensions(0);
  unsigned int height = fileListIO->GetDimensions(1);
  const char* fourCC = "MP42";
  unsigned int nChannels = fileListIO->GetNumberOfComponents();

  // Reset the VideoIO
  fileListIO->FinishReadingOrWriting();


  /////////////////////////////////////////////////////////////////////////////
  // Test Writing
  //


  //////
  // SetWriterParameters
  //////
  std::cout << "FileListVideoIO::SetWriterParameters..." << std::endl;

  // Reset the saved parameters
  std::vector<unsigned int> size;
  size.push_back(width);
  size.push_back(height);
  fileListIO->SetWriterParameters(fps, size, fourCC, nChannels, itk::ImageIOBase::UCHAR);

  // Make sure they set correctly
  if (fileListIO->GetFpS() != fps || fileListIO->GetDimensions(0) != width ||
      fileListIO->GetDimensions(1) != height || fileListIO->GetNumberOfComponents() != nChannels)
    {
    std::cerr << "Didn't set writer parmeters correctly" << std::endl;
    std::cerr << "  FpS -> Got: " << fileListIO->GetFpS() << " Expected: " << fps
              << std::endl;
    std::cerr << "  width -> Got: " << fileListIO->GetDimensions(0) << " Expected: "
              << width << std::endl;
    std::cerr << "  height -> Got: " << fileListIO->GetDimensions(1) << " Expected: "
              << height << std::endl;
    std::cerr << "  NChannels -> Got: " << fileListIO->GetNumberOfComponents()
              << " Expected: " << nChannels << std::endl;
    ret = EXIT_FAILURE;
    }

  //////
  // CanWriteFile
  //////
  std::cout << "FileListVideoIO::CanWriteFile..." << std::endl;

  // Test CanWriteFile on good filename
  if (!fileListIO->CanWriteFile(output))
    {
    std::cerr << "CanWriteFile didn't return true correctly" << std::endl;
    ret = EXIT_FAILURE;
    }

  // Test CanWriteFile on bad filename
  if (fileListIO->CanWriteFile("asdfasdfasdf"))
    {
    std::cerr << "CanWriteFile should have failed for bad filename" << std::endl;
    ret = EXIT_FAILURE;
    }


  //////
  // Write
  //////
  std::cout << "FileListVideoIO::Write..." << std::endl;

  // Set output filename
  fileListIO->SetFileName( output );

  // Set up a two more VideoIOs to read while we're writing
  itk::FileListVideoIO::Pointer fileListIO2 = itk::FileListVideoIO::New();
  itk::FileListVideoIO::Pointer fileListIO3 = itk::FileListVideoIO::New();
  fileListIO2->SetFileName( input );
  fileListIO2->ReadImageInformation();
  fileListIO3->SetFileName( output );

  // Loop through all frames to read with fileListIO2 and write with fileListIO
  for (unsigned int i = 0; i < inNumFrames; ++i)
    {
    // Set up a buffer to read to
    size_t bufferSize = fileListIO2->GetImageSizeInBytes();
    PixelType buffer[bufferSize];

    // Read into the buffer
    fileListIO2->Read(static_cast<void*>(buffer));

    // Write out the frame from the buffer
    fileListIO->Write(static_cast<void*>(buffer));

    // Now, read back in from the written file and make sure the buffers match
    fileListIO3->ReadImageInformation();
    PixelType reReadBuffer[bufferSize];
    fileListIO3->Read(static_cast<void*>(reReadBuffer));
    if (memcmp(reinterpret_cast<void*>(buffer), reinterpret_cast<void*>(reReadBuffer), bufferSize))
      {
      std::cerr << "Didn't write correctly for frame " << i << std::endl;
      ret = EXIT_FAILURE;
      }
    }

  // Finish writing
  fileListIO2->FinishReadingOrWriting();
  fileListIO->FinishReadingOrWriting();


  std::cout<<"Done !"<<std::endl;
  return ret;
}

int itkFileListVideoIOTest ( int argc, char *argv[] )
{
  if (argc != 9)
    {
    std::cerr << "Usage: [Video Input] [Non-Video Input] [Video Output] [Webcam Output] "
      "[Width] [Height] [Num Frames] [FpS]" << std::endl;
    return EXIT_FAILURE;
    }

  return test_FileListVideoIO(argv[1], argv[2], argv[3], argv[4], atoi(argv[5]), atoi(argv[6]),
                            atoi(argv[7]), atof(argv[8]));
}

