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

#pragma once

#include "OpenAPIBaseModel.h"

namespace IMSZeuzAPI
{

/*
 * OpenAPIPayloadPortV0
 *
 * 
 */
class IMSZEUZAPI_API OpenAPIPayloadPortV0 : public Model
{
public:
    virtual ~OpenAPIPayloadPortV0() {}
	bool FromJson(const TSharedPtr<FJsonValue>& JsonValue) final;
	void WriteJson(JsonWriter& Writer) const final;

	/* Port name, as defined in the payload specification. */
	FString Name;
	/* Port number assigned to the host port. */
	int32 Port = 0;
};

}
