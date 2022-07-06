/**
 * Payload Local API
 * The Payload Local API available in your running payloads. It provides an interface between your game server and the IMS zeuz orchestration service.  # Warning The Payload Local API is starting up at the same time as your game server, and may not be initially available. Use a retry mechanism to ensure the request is successful.  # OpenAPI Best practices The Payload local API is using the OpenAPI standard, it is advised to use an OpenAPI client generator for the language your game server uses. See a [list of OpenAPI client generators](https://openapi-generator.tech/docs/generators/).  # Address The Payload Local API address can be obtained using an environment variable: `http://${ORCHESTRATION_PAYLOAD_API}`.  # Authentication As the Payload Local API is only accessible from within the payload, it does not require authentication. 
 *
 * OpenAPI spec version: 0.0.1
 * 
 *
 * NOTE: This class is auto generated by OpenAPI Generator
 * https://github.com/OpenAPITools/openapi-generator
 * Do not edit the class manually.
 */

#include "OpenAPIHelpers.h"

#include "IMSZeuzAPIModule.h"

#include "Interfaces/IHttpRequest.h"
#include "PlatformHttp.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace IMSZeuzAPI
{

HttpFileInput::HttpFileInput(const TCHAR* InFilePath)
{
	SetFilePath(InFilePath);
}

HttpFileInput::HttpFileInput(const FString& InFilePath)
{
	SetFilePath(InFilePath);
}

void HttpFileInput::SetFilePath(const TCHAR* InFilePath)
{
	FilePath = InFilePath;
	if(ContentType.IsEmpty())
	{
		ContentType = FPlatformHttp::GetMimeType(InFilePath);
	}
}

void HttpFileInput::SetFilePath(const FString& InFilePath)
{
	SetFilePath(*InFilePath);
}

void HttpFileInput::SetContentType(const TCHAR* InContentType)
{
	ContentType = InContentType;
}

FString HttpFileInput::GetFilename() const
{
	return FPaths::GetCleanFilename(FilePath);
}

//////////////////////////////////////////////////////////////////////////

const TCHAR* HttpMultipartFormData::Delimiter = TEXT("--");
const TCHAR* HttpMultipartFormData::Newline = TEXT("\r\n");

void HttpMultipartFormData::SetBoundary(const TCHAR* InBoundary)
{
	checkf(Boundary.IsEmpty(), TEXT("Boundary must be set before usage"));
	Boundary = InBoundary;
}

const FString& HttpMultipartFormData::GetBoundary() const
{
	if (Boundary.IsEmpty())
	{
		// Generate a random boundary with enough entropy, should avoid occurrences of the boundary in the data.
		// Since the boundary is generated at every request, in case of failure, retries should succeed.
		Boundary = FGuid::NewGuid().ToString(EGuidFormats::Short);
	}

	return Boundary;
}

void HttpMultipartFormData::SetupHttpRequest(const FHttpRequestRef& HttpRequest)
{
	if(HttpRequest->GetVerb() != TEXT("POST"))
	{
		UE_LOG(LogIMSZeuzAPI, Error, TEXT("Expected POST verb when using multipart form data"));
	}

	// Append final boundary
	AppendString(Delimiter);
	AppendString(*GetBoundary());
	AppendString(Delimiter);

	HttpRequest->SetHeader("Content-Type", FString::Printf(TEXT("multipart/form-data; boundary=%s"), *GetBoundary()));
	HttpRequest->SetContent(FormData);
}

void HttpMultipartFormData::AddStringPart(const TCHAR* Name, const TCHAR* Data)
{
	// Add boundary
	AppendString(Delimiter);
	AppendString(*GetBoundary());
	AppendString(Newline);

	// Add header
	AppendString(*FString::Printf(TEXT("Content-Disposition: form-data; name = \"%s\""), Name));
	AppendString(Newline);
	AppendString(*FString::Printf(TEXT("Content-Type: text/plain; charset=utf-8")));
	AppendString(Newline);

	// Add header to body splitter
	AppendString(Newline);

	// Add Data
	AppendString(Data);
	AppendString(Newline);
}

void HttpMultipartFormData::AddJsonPart(const TCHAR* Name, const FString& JsonString)
{
	// Add boundary
	AppendString(Delimiter);
	AppendString(*GetBoundary());
	AppendString(Newline);

	// Add header
	AppendString(*FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\""), Name));
	AppendString(Newline);
	AppendString(*FString::Printf(TEXT("Content-Type: application/json; charset=utf-8")));
	AppendString(Newline);

	// Add header to body splitter
	AppendString(Newline);

	// Add Data
	AppendString(*JsonString);
	AppendString(Newline);
}

void HttpMultipartFormData::AddBinaryPart(const TCHAR* Name, const TArray<uint8>& ByteArray)
{
	// Add boundary
	AppendString(Delimiter);
	AppendString(*GetBoundary());
	AppendString(Newline);

	// Add header
	AppendString(*FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\""), Name));
	AppendString(Newline);
	AppendString(*FString::Printf(TEXT("Content-Type: application/octet-stream")));
	AppendString(Newline);

	// Add header to body splitter
	AppendString(Newline);

	// Add Data
	FormData.Append(ByteArray);
	AppendString(Newline);
}

void HttpMultipartFormData::AddFilePart(const TCHAR* Name, const HttpFileInput& File)
{
	TArray<uint8> FileContents;
	if (!FFileHelper::LoadFileToArray(FileContents, *File.GetFilePath()))
	{
		UE_LOG(LogIMSZeuzAPI, Error, TEXT("Failed to load file (%s)"), *File.GetFilePath());
		return;
	}

	// Add boundary
	AppendString(Delimiter);
	AppendString(*GetBoundary());
	AppendString(Newline);

	// Add header
	AppendString(*FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\""), Name, *File.GetFilename()));
	AppendString(Newline);
	AppendString(*FString::Printf(TEXT("Content-Type: %s"), *File.GetContentType()));
	AppendString(Newline);

	// Add header to body splitter
	AppendString(Newline);

	// Add Data
	FormData.Append(FileContents);
	AppendString(Newline);
}

void HttpMultipartFormData::AppendString(const TCHAR* Str)
{
	FTCHARToUTF8 utf8Str(Str);
	FormData.Append((uint8*)utf8Str.Get(), utf8Str.Length());
}

//////////////////////////////////////////////////////////////////////////

bool ParseDateTime(const FString& DateTimeString, FDateTime& OutDateTime)
{
	// Iso8601 Format: 	DateTime: YYYY-mm-ddTHH:MM:SS(.sss)(Z|+hh:mm|+hhmm|-hh:mm|-hhmm)
	{
		// We cannot call directly FDateTime::ParseIso8601 because it does not allow for precision beyond the millisecond, but DateTimeString might have more digits
		int32 DotIndex;
		FString StringToParse = DateTimeString;
		if (DateTimeString.FindChar('.', DotIndex))
		{
			int32 TimeZoneIndex;
			if (DateTimeString.FindChar('Z', TimeZoneIndex) || DateTimeString.FindChar('+', TimeZoneIndex) || DateTimeString.FindChar('-', TimeZoneIndex))
			{
				// The string contains a time zone designator starting at TimeZoneIndex
				if (TimeZoneIndex > DotIndex + 4)
				{
					// Trim to millisecond
					StringToParse = DateTimeString.Left(DotIndex + 4) + DateTimeString.RightChop(TimeZoneIndex);
				}
			}
			else
			{
				// the string does not contain a time zone designator, trim it to the millisecond
				StringToParse = DateTimeString.Left(DotIndex + 4);
			}
		}

		if (FDateTime::ParseIso8601(*StringToParse, OutDateTime))
			return true;
	}

	if (FDateTime::ParseHttpDate(DateTimeString, OutDateTime))
		return true;

	return FDateTime::Parse(DateTimeString, OutDateTime);
}

}
