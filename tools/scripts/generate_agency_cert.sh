#!/bin/bash
set -e

function LOG_ERROR()
{
    local content=${1}
    echo -e "\033[31m"${content}"\033[0m"
}

function LOG_INFO()
{
    local content=${1}
    echo -e "\033[34m"${content}"\033[0m"
}

function execute_cmd()
{
    local command="${1}"
    eval ${command}
    local ret=$?
    if [ $ret -ne 0 ];then
        LOG_ERROR "execute command ${command} FAILED"
        exit 1
    else
        LOG_INFO "execute command ${command} SUCCESS"
    fi
}

yes_go_other_exit()
{
    read -r -p "[Y/n]: " input
    case $input in
        [yY][eE][sS]|[yY])
            ;;

        [nN][oO]|[nN])
            exit 1
                ;;

        *)
        exit 1
        ;;
    esac    
}

output_dir=
ca_dir=
manual_input=
agency_name=

this_script=$0
help() {
    LOG_ERROR "${1}"
    LOG_INFO "Usage:"
    LOG_INFO "    -c <ca dir>         The dir of ca.crt and ca.key "
    LOG_INFO "    -o <output dir>     Where agency.crt agency.key generate "
    LOG_INFO "    -n <agency name>    Name of agency"
    LOG_INFO "    -m                  Input agency information manually"
    LOG_INFO "    -g                  Generate agency certificates with guomi algorithms"
    LOG_INFO "    -d                  The Path of Guomi Directory"
    LOG_INFO "    -h                  This help"
    LOG_INFO "Example:"
    LOG_INFO "    bash $this_script -c /mydata -o /mydata -n fisco-dev"
    LOG_INFO "    bash $this_script -c /mydata -o /mydata -n fisco-dev -m"
    LOG_INFO "guomi Example:"
    LOG_INFO "    bash $this_script -c /mydata -o /mydata -n fisco-dev -g"
    LOG_INFO "    bash $this_script -c /mydata -o /mydata -n fisco-dev -m -g" 
exit -1
}

enable_guomi=0
guomi_dir=../cert/GM
while getopts "c:o:n:d:gmh" option;do
	case $option in
    c) ca_dir=$OPTARG;;
	o) output_dir=$OPTARG;;
    n) agency_name=$OPTARG;;
    d) guomi_dir=${OPTARG};;
    g) enable_guomi=1;;
    m) manual_input="yes";;
	h) help;;
	esac
done

[ -z $ca_dir ] && help 'Error: Please specify <ca dir> using -c'
[ -z $output_dir ] && help 'Error: Please specify <output dir> using -o'
[ -z $agency_name ] && help 'Error: Please specify <agency name> using -n'

generate_cert_config() {
    output_dir=$1
    echo '
[ca]
default_ca=default_ca
[default_ca]
default_days = 365
default_md = sha256
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
[req_distinguished_name]
countryName = CN
countryName_default = CN
stateOrProvinceName = State or Province Name (full name)
stateOrProvinceName_default =GuangDong
localityName = Locality Name (eg, city)
localityName_default = ShenZhen
organizationalUnitName = Organizational Unit Name (eg, section)
organizationalUnitName_default = test_org
commonName =  Organizational  commonName (eg, test_org)
commonName_default = test_org
commonName_max = 64
[ v3_req ]
# Extensions to add to a certificate request
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
[ v4_req ]
basicConstraints = CA:TRUE
' > $output_dir/cert.cnf
}

generate_cert() {
    output_dir=$1
    cp $ca_dir/ca.crt $output_dir/
    [ -n "$manual_input" ] && {
        openssl genrsa -out $output_dir/agency.key 2048
        openssl req -new  -key $output_dir/agency.key -out $output_dir/agency.csr
        openssl x509 -req -days 3650 -CA $ca_dir/ca.crt -CAkey $ca_dir/ca.key -CAcreateserial -in $output_dir/agency.csr -out $output_dir/agency.crt  -extensions v4_req 
        return
    }

    generate_cert_config $output_dir
    openssl genrsa -out $output_dir/agency.key 2048
    openssl req -new  -key $output_dir/agency.key -config $output_dir/cert.cnf -out $output_dir/agency.csr -batch
    openssl x509 -req -days 3650 -CA $ca_dir/ca.crt -CAkey $ca_dir/ca.key -CAcreateserial -in $output_dir/agency.csr -out $output_dir/agency.crt  -extensions v4_req -extfile $output_dir/cert.cnf

}

current_dir=`pwd`
function generate_guomi_cert() {
    local agency_name="${1}"
    local agency_dir="${2}"
    local ca_dir="${3}"
    local guomi_dir="${4}"
    local manual="no"
    if [ "${manual_input}" == "yes" ];then
        manual="yes"
    fi
    execute_cmd "cd ${guomi_dir}"
    execute_cmd "chmod a+x gmagency.sh"
    execute_cmd "./gmagency.sh ${agency_name} ${agency_dir} ${ca_dir} ${manual}"
    execute_cmd "cd ${current_dir}"
}

##generate guomi cert
if [ ${enable_guomi} -eq 1 ];then
    generate_guomi_cert ${agency_name} ${output_dir} ${ca_dir} ${guomi_dir}
    exit 0
fi


name=$output_dir/$agency_name  
if [ -z "$name" ];  then
    help "Error: Please input agency name!"
    exit 1
elif [  -d "$name" ]; then
    echo "Attention! Duplicate generation of \"$name\". Overwrite?"
    yes_go_other_exit
fi
mkdir -p $name
generate_cert $name
LOG_INFO "Generate success! Agency keys has been generated into "$name

