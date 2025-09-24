#!/bin/zsh
set -eo pipefail

# Import utility functions
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/utils.sh"

stack_name=$(get_stack_name)

log_info "Deploying CloudFormation stack: $stack_name"

aws cloudformation deploy --template-file template.yaml --stack-name "$stack_name" --capabilities CAPABILITY_IAM --capabilities CAPABILITY_NAMED_IAM

log_info "Creating IoT certificates and keys..."
CERT_ARN=$(aws iot create-keys-and-certificate --set-as-active --certificate-pem-outfile certificate.pem.crt --private-key-outfile private.pem.key | jq -r '.certificateArn')

log_info "Attaching certificate to thing and policy..."
policy_name=$(aws cloudformation describe-stack-resource --stack-name "$stack_name" --logical-resource-id XiaoESP32GatewayPolicy --query 'StackResourceDetail.PhysicalResourceId' --output text)
aws iot attach-policy --policy-name $policy_name --target $CERT_ARN

thing_name=$(aws cloudformation describe-stack-resource --stack-name "$stack_name" --logical-resource-id XiaoESP32GatewayThing --query 'StackResourceDetail.PhysicalResourceId' --output text)
aws iot attach-thing-principal --thing-name $thing_name --principal $CERT_ARN

IOT_ENDPOINT=$(aws iot describe-endpoint --endpoint-type iot:Data-ATS --query endpointAddress --output text)

log_info "Setup complete!"
log_info "Thing Name: $thing_name"
log_info "Certificate files saved: certificate.pem.crt, private.pem.key"
log_info "IoT Endpoint: $IOT_ENDPOINT"
