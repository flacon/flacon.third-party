#include "All.h"
#include "WAVInputSource.h"
#include IO_HEADER_FILE
#include "MACLib.h"
#include "GlobalFunctions.h"

struct RIFF_HEADER 
{
    char cRIFF[4];            // the characters 'RIFF' indicating that it's a RIFF file
    uint32 nBytes;    // the number of bytes following this header
};

struct DATA_TYPE_ID_HEADER 
{
    char cDataTypeID[4];    // should equal 'WAVE' for a WAV file
};

struct WAV_FORMAT_HEADER
{
    uint16 nFormatTag;            // the format of the WAV...should equal 1 for a PCM file
    uint16 nChannels;            // the number of channels
    uint32 nSamplesPerSecond;    // the number of samples per second
    uint32 nBytesPerSecond;        // the bytes per second
    uint16 nBlockAlign;            // block alignment
    uint16 nBitsPerSample;        // the number of bits per sample
};

struct RIFF_CHUNK_HEADER
{
    char cChunkLabel[4];        // should equal "data" indicating the data chunk
    uint32 nChunkBytes;  // the bytes of the chunk  
};

#ifdef SHNTOOL
unsigned long UCHAR_TO_ULONG_LE(unsigned char * buf)
/* converts 4 bytes stored in little-endian format to an unsigned long */
{
	return (unsigned long)((buf[3] << 24) + (buf[2] << 16) + (buf[1] << 8) + buf[0]);
}
 
unsigned short UCHAR_TO_USHORT_LE(unsigned char * buf)
/* converts 2 bytes stored in little-endian format to an unsigned short */
{
	return (unsigned short)((buf[1] << 8) + buf[0]);
}
#endif

CInputSource * CreateInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int * pHeaderBytes, int * pTerminatingBytes, int * pErrorCode)
{ 
#ifdef SHNTOOL
	return new CWAVInputSource(pSourceName, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode);
#else
    // error check the parameters
    if ((pSourceName == NULL) || (wcslen(pSourceName) == 0))
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return NULL;
    }

    // get the extension
    const wchar_t * pExtension = &pSourceName[wcslen(pSourceName)];
    while ((pExtension > pSourceName) && (*pExtension != '.'))
        pExtension--;

    // create the proper input source
    if (wcsicmp(pExtension, L".wav") == 0)
    {
        if (pErrorCode) *pErrorCode = ERROR_SUCCESS;
        return new CWAVInputSource(pSourceName, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode);
    }
    else
    {
        if (pErrorCode) *pErrorCode = ERROR_INVALID_INPUT_FILE;
        return NULL;
    }
#endif
}

CWAVInputSource::CWAVInputSource(CIO * pIO, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int * pHeaderBytes, int * pTerminatingBytes, int * pErrorCode)
    : CInputSource(pIO, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode)
{
    m_bIsValid = FALSE;

    if (pIO == NULL || pwfeSource == NULL)
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return;
    }
    
    m_spIO.Assign(pIO, FALSE, FALSE);

    int nRetVal = AnalyzeSource();
    if (nRetVal == ERROR_SUCCESS)
    {
        // fill in the parameters
        if (pwfeSource) memcpy(pwfeSource, &m_wfeSource, sizeof(WAVEFORMATEX));
        if (pTotalBlocks) *pTotalBlocks = m_nDataBytes / m_wfeSource.nBlockAlign;
        if (pHeaderBytes) *pHeaderBytes = m_nHeaderBytes;
        if (pTerminatingBytes) *pTerminatingBytes = m_nTerminatingBytes;

        m_bIsValid = TRUE;
    }
    
    if (pErrorCode) *pErrorCode = nRetVal;
}

CWAVInputSource::CWAVInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int * pHeaderBytes, int * pTerminatingBytes, int * pErrorCode)
    : CInputSource(pSourceName, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode)
{
    m_bIsValid = FALSE;

    if (pSourceName == NULL || pwfeSource == NULL)
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return;
    }
    
    m_spIO.Assign(new IO_CLASS_NAME);
    if (m_spIO->Open(pSourceName) != ERROR_SUCCESS)
    {
        m_spIO.Delete();
        if (pErrorCode) *pErrorCode = ERROR_INVALID_INPUT_FILE;
        return;
    }

    int nRetVal = AnalyzeSource();
    if (nRetVal == ERROR_SUCCESS)
    {
        // fill in the parameters
        if (pwfeSource) memcpy(pwfeSource, &m_wfeSource, sizeof(WAVEFORMATEX));
        if (pTotalBlocks) *pTotalBlocks = m_nDataBytes / m_wfeSource.nBlockAlign;
        if (pHeaderBytes) *pHeaderBytes = m_nHeaderBytes;
        if (pTerminatingBytes) *pTerminatingBytes = m_nTerminatingBytes;

        m_bIsValid = TRUE;
    }
    
    if (pErrorCode) *pErrorCode = nRetVal;
}

CWAVInputSource::~CWAVInputSource()
{


}

static void swap_wave_header(WAV_FORMAT_HEADER *header)
{
    header->nFormatTag = swap_int16(header->nFormatTag);
    header->nChannels = swap_int16(header->nChannels);
    header->nSamplesPerSecond = swap_int32(header->nSamplesPerSecond);
    header->nBytesPerSecond = swap_int32(header->nBytesPerSecond);
    header->nBlockAlign = swap_int16(header->nBlockAlign);
    header->nBitsPerSample = swap_int16(header->nBitsPerSample);
}

int CWAVInputSource::AnalyzeSource()
{
#ifdef SHNTOOL
	unsigned char *p = m_sFullHeader, *priff = NULL;

	// seek to the beginning (just in case)
	m_spIO->Seek(0, FILE_BEGIN);

	// get the file size
	m_nFileBytes = m_spIO->GetSize();

	// get the RIFF header
	RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_HEADER)))

	// make sure the RIFF header is valid
	if (!(p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F'))
		return ERROR_INVALID_INPUT_FILE;
	p += sizeof(RIFF_HEADER);

	// read the data type header
	RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(DATA_TYPE_ID_HEADER)))

	// make sure it's the right data type
	if (!(p[0] == 'W' && p[1] == 'A' && p[2] == 'V' && p[3] == 'E'))
		return ERROR_INVALID_INPUT_FILE;
	p += sizeof(DATA_TYPE_ID_HEADER);

	// find the 'fmt ' chunk
	RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))

	while (!(p[0] == 'f' && p[1] == 'm' && p[2] == 't' && p[3] == ' ')) 
	{
		p += sizeof(RIFF_CHUNK_HEADER);

		// move the file pointer to the end of this chunk
		RETURN_ON_ERROR(ReadSafe(m_spIO, p, UCHAR_TO_ULONG_LE(p+4)))
		p += UCHAR_TO_ULONG_LE(p+4);

		// check again for the data chunk
		RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))
    }

	priff = p+4;
	p += sizeof(RIFF_CHUNK_HEADER);

	// read the format info
	RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(WAV_FORMAT_HEADER)))

	// error check the header to see if we support it
	if (UCHAR_TO_USHORT_LE(p) != 1)
		return ERROR_INVALID_INPUT_FILE;

	// copy the format information to the WAVEFORMATEX passed in
	FillWaveFormatEx(&m_wfeSource, UCHAR_TO_ULONG_LE(p+4), UCHAR_TO_USHORT_LE(p+14), UCHAR_TO_USHORT_LE(p+2));

	p += sizeof(WAV_FORMAT_HEADER);

	// skip over any extra data in the header
	int nWAVFormatHeaderExtra = UCHAR_TO_ULONG_LE(priff) - sizeof(WAV_FORMAT_HEADER);
	if (nWAVFormatHeaderExtra < 0)
		return ERROR_INVALID_INPUT_FILE;
	else {
		RETURN_ON_ERROR(ReadSafe(m_spIO, p, nWAVFormatHeaderExtra))
		p += nWAVFormatHeaderExtra;
	}

	// find the data chunk
	RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))

	while (!(p[0] == 'd' && p[1] == 'a' && p[2] == 't' && p[3] == 'a')) 
	{
		p += sizeof(RIFF_CHUNK_HEADER);

		// move the file pointer to the end of this chunk
		RETURN_ON_ERROR(ReadSafe(m_spIO, p, UCHAR_TO_ULONG_LE(p+4)))
		p += UCHAR_TO_ULONG_LE(p+4);

		// check again for the data chunk
		RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER))) 
	}

	// we're at the data block
	m_nDataBytes = UCHAR_TO_ULONG_LE(p+4);
	if (m_nDataBytes < 0)
		m_nDataBytes = m_nFileBytes - m_nHeaderBytes;

	p += sizeof(RIFF_CHUNK_HEADER);

	m_nHeaderBytes = p - m_sFullHeader;

	// make sure the data bytes is a whole number of blocks
	if ((m_nDataBytes % m_wfeSource.nBlockAlign) != 0)
		return ERROR_INVALID_INPUT_FILE;

	// calculate the terminating byts
	m_nTerminatingBytes = 0;
#else
    // seek to the beginning (just in case)
    m_spIO->Seek(0, FILE_BEGIN);
    
    // get the file size
    m_nFileBytes = m_spIO->GetSize();

    // get the RIFF header
    RIFF_HEADER RIFFHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFHeader, sizeof(RIFFHeader))) 
    RIFFHeader.nBytes = swap_int32(RIFFHeader.nBytes);

    // make sure the RIFF header is valid
    if (!(RIFFHeader.cRIFF[0] == 'R' && RIFFHeader.cRIFF[1] == 'I' && RIFFHeader.cRIFF[2] == 'F' && RIFFHeader.cRIFF[3] == 'F')) 
        return ERROR_INVALID_INPUT_FILE;

    // read the data type header
    DATA_TYPE_ID_HEADER DataTypeIDHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &DataTypeIDHeader, sizeof(DataTypeIDHeader))) 
    
    // make sure it's the right data type
    if (!(DataTypeIDHeader.cDataTypeID[0] == 'W' && DataTypeIDHeader.cDataTypeID[1] == 'A' && DataTypeIDHeader.cDataTypeID[2] == 'V' && DataTypeIDHeader.cDataTypeID[3] == 'E')) 
        return ERROR_INVALID_INPUT_FILE;

    // find the 'fmt ' chunk
    RIFF_CHUNK_HEADER RIFFChunkHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
        RIFFChunkHeader.nChunkBytes = swap_int32(RIFFChunkHeader.nChunkBytes);
    
    while (!(RIFFChunkHeader.cChunkLabel[0] == 'f' && RIFFChunkHeader.cChunkLabel[1] == 'm' && RIFFChunkHeader.cChunkLabel[2] == 't' && RIFFChunkHeader.cChunkLabel[3] == ' ')) 
    {
        // move the file pointer to the end of this chunk
        m_spIO->Seek(RIFFChunkHeader.nChunkBytes, FILE_CURRENT);

        // check again for the data chunk
        RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
	RIFFChunkHeader.nChunkBytes = swap_int32(RIFFChunkHeader.nChunkBytes);
    }
    
    // read the format info
    WAV_FORMAT_HEADER WAVFormatHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &WAVFormatHeader, sizeof(WAVFormatHeader))) 
	swap_wave_header(&WAVFormatHeader);

    // error check the header to see if we support it
    if (WAVFormatHeader.nFormatTag != 1)
        return ERROR_INVALID_INPUT_FILE;

    // copy the format information to the WAVEFORMATEX passed in
    FillWaveFormatEx(&m_wfeSource, WAVFormatHeader.nSamplesPerSecond, WAVFormatHeader.nBitsPerSample, WAVFormatHeader.nChannels);

    // skip over any extra data in the header
    int nWAVFormatHeaderExtra = RIFFChunkHeader.nChunkBytes - sizeof(WAVFormatHeader);
    if (nWAVFormatHeaderExtra < 0)
        return ERROR_INVALID_INPUT_FILE;
    else
        m_spIO->Seek(nWAVFormatHeaderExtra, FILE_CURRENT);
    
    // find the data chunk
    RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
	RIFFChunkHeader.nChunkBytes = swap_int32(RIFFChunkHeader.nChunkBytes);

    while (!(RIFFChunkHeader.cChunkLabel[0] == 'd' && RIFFChunkHeader.cChunkLabel[1] == 'a' && RIFFChunkHeader.cChunkLabel[2] == 't' && RIFFChunkHeader.cChunkLabel[3] == 'a')) 
    {
        // move the file pointer to the end of this chunk
        m_spIO->Seek(RIFFChunkHeader.nChunkBytes, FILE_CURRENT);

        // check again for the data chunk
        RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
	RIFFChunkHeader.nChunkBytes = swap_int32(RIFFChunkHeader.nChunkBytes);
    }

    // we're at the data block
    m_nHeaderBytes = m_spIO->GetPosition();
    m_nDataBytes = RIFFChunkHeader.nChunkBytes;
    if (m_nDataBytes < 0)
        m_nDataBytes = m_nFileBytes - m_nHeaderBytes;

    // make sure the data bytes is a whole number of blocks
    if ((m_nDataBytes % m_wfeSource.nBlockAlign) != 0)
        return ERROR_INVALID_INPUT_FILE;

    // calculate the terminating byts
    m_nTerminatingBytes = m_nFileBytes - m_nDataBytes - m_nHeaderBytes;
#endif

	// we made it this far, everything must be cool
	return ERROR_SUCCESS;
}

int CWAVInputSource::GetData(unsigned char * pBuffer, int nBlocks, int * pBlocksRetrieved)
{
    if (!m_bIsValid) return ERROR_UNDEFINED;

    int nBytes = (m_wfeSource.nBlockAlign * nBlocks);
    unsigned int nBytesRead = 0;

#ifdef SHNTOOL
    int nRetVal = m_spIO->Read(pBuffer, nBytes, &nBytesRead);
#else
    if (m_spIO->Read(pBuffer, nBytes, &nBytesRead) != ERROR_SUCCESS)
        return ERROR_IO_READ;
#endif

    if (pBlocksRetrieved) *pBlocksRetrieved = (nBytesRead / m_wfeSource.nBlockAlign);

#ifdef SHNTOOL
    if (nRetVal != ERROR_SUCCESS)
        return ERROR_IO_READ;
#endif

    return ERROR_SUCCESS;
}

int CWAVInputSource::GetHeaderData(unsigned char * pBuffer)
{
#ifdef SHNTOOL
	int i;

	if (!m_bIsValid) return ERROR_UNDEFINED;

	for (i=0;i<m_nHeaderBytes;i++)
		*(pBuffer + i) = *(m_sFullHeader + i);

	return ERROR_SUCCESS;
#else
    if (!m_bIsValid) return ERROR_UNDEFINED;

    int nRetVal = ERROR_SUCCESS;

    if (m_nHeaderBytes > 0)
    {
        int nOriginalFileLocation = m_spIO->GetPosition();

        m_spIO->Seek(0, FILE_BEGIN);
        
        unsigned int nBytesRead = 0;
        int nReadRetVal = m_spIO->Read(pBuffer, m_nHeaderBytes, &nBytesRead);

        if ((nReadRetVal != ERROR_SUCCESS) || (m_nHeaderBytes != int(nBytesRead)))
        {
            nRetVal = ERROR_UNDEFINED;
        }

        m_spIO->Seek(nOriginalFileLocation, FILE_BEGIN);
    }

    return nRetVal;
#endif
}

int CWAVInputSource::GetTerminatingData(unsigned char * pBuffer)
{
    if (!m_bIsValid) return ERROR_UNDEFINED;

    int nRetVal = ERROR_SUCCESS;

    if (m_nTerminatingBytes > 0)
    {
        int nOriginalFileLocation = m_spIO->GetPosition();

        m_spIO->Seek(-m_nTerminatingBytes, FILE_END);
        
        unsigned int nBytesRead = 0;
        int nReadRetVal = m_spIO->Read(pBuffer, m_nTerminatingBytes, &nBytesRead);

        if ((nReadRetVal != ERROR_SUCCESS) || (m_nTerminatingBytes != int(nBytesRead)))
        {
            nRetVal = ERROR_UNDEFINED;
        }

        m_spIO->Seek(nOriginalFileLocation, FILE_BEGIN);
    }

    return nRetVal;
}
