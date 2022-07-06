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

#include "OpenAPIPayloadV0.h"

#include "IMSZeuzAPIModule.h"
#include "OpenAPIHelpers.h"

#include "Templates/SharedPointer.h"

namespace IMSZeuzAPI
{

void OpenAPIPayloadV0::WriteJson(JsonWriter& Writer) const
{
	Writer->WriteObjectStart();
	Writer->WriteIdentifierPrefix(TEXT("id")); WriteJsonValue(Writer, Id);
	Writer->WriteIdentifierPrefix(TEXT("cluster_id")); WriteJsonValue(Writer, ClusterId);
	Writer->WriteIdentifierPrefix(TEXT("allocation_id")); WriteJsonValue(Writer, AllocationId);
	Writer->WriteIdentifierPrefix(TEXT("status")); WriteJsonValue(Writer, Status);
	Writer->WriteIdentifierPrefix(TEXT("created")); WriteJsonValue(Writer, Created);
	Writer->WriteIdentifierPrefix(TEXT("metadata")); WriteJsonValue(Writer, Metadata);
	Writer->WriteObjectEnd();
}

bool OpenAPIPayloadV0::FromJson(const TSharedPtr<FJsonValue>& JsonValue)
{
	const TSharedPtr<FJsonObject>* Object;
	if (!JsonValue->TryGetObject(Object))
		return false;

	bool ParseSuccess = true;

	ParseSuccess &= TryGetJsonValue(*Object, TEXT("id"), Id);
	ParseSuccess &= TryGetJsonValue(*Object, TEXT("cluster_id"), ClusterId);
	ParseSuccess &= TryGetJsonValue(*Object, TEXT("allocation_id"), AllocationId);
	ParseSuccess &= TryGetJsonValue(*Object, TEXT("status"), Status);
	ParseSuccess &= TryGetJsonValue(*Object, TEXT("created"), Created);
	ParseSuccess &= TryGetJsonValue(*Object, TEXT("metadata"), Metadata);

	return ParseSuccess;
}

}
