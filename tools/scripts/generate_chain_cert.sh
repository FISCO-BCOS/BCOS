#!/bin/bash
set -e
ca_dir=
manual_input=
cert_cnf=
this_script=$0

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

help() {
    LOG_ERROR "${1}"
    LOG_INFO "Usage:"
    LOG_INFO "    -o <ca dir>     Where ca.crt ca.key generate "
    LOG_INFO "    -m                  Input ca information manually"
    LOG_INFO "    -g                  Generate chain certificates with guomi algorithms"
    LOG_INFO "    -d                  The Path of Guomi Directory"
    LOG_INFO "    -h                  This help"
    LOG_INFO "Example:"
    LOG_INFO "    bash $this_script -o /mydata -m"
    LOG_INFO "    bash $this_script -o /mydata "
    LOG_INFO "guomi Example:"
    LOG_INFO "    bash $this_script -o /mydata -m -g"
    LOG_INFO "    bash $this_script -o /mydata -g"
exit -1
}

enable_guomi=0
guomi_dir=../cert/GM
while getopts "o:c:d:mgh" option;do
	case $option in
	o) ca_dir=$OPTARG;;
    c) cert_cnf=${OPTARG};;
    ## for guomi support
    #guomi script directory, default is current directory
    d) guomi_dir=${OPTARG};;
    g) enable_guomi=1;;
    m) manual_input="yes";;
	h) help;;
	esac
done

[ -z $ca_dir ] && help 'Error: Please specify <ca dir> using -o'


generate_cert_config() {
    ca_dir=$1
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
' > $ca_dir/cert.cnf
}

## generate ECDSA cert
generate_cert() {
    ca_dir=$1

    [ -n "$manual_input" ] && {
        openssl genrsa -out $ca_dir/ca.key 2048
        openssl req -new -x509 -days 3650 -key $ca_dir/ca.key -out $ca_dir/ca.crt
        return
    }
    generate_cert_config ${ca_dir}
    openssl genrsa -out $ca_dir/ca.key 2048
    openssl req -new -x509 -days 3650 -key $ca_dir/ca.key -out $ca_dir/ca.crt -config $ca_dir/cert.cnf -batch   
}

## generate guomi cert
current_dir=`pwd`
function generate_guomi_cert() {
    local dst_dir="${1}"
    local guomi_dir="${2}"
    local manual="no"
    if [ "${manual_input}" == "yes" ];then
        manual="yes"
    fi
    if [ ! -f "${guomi_dir}/cert.cnf" ];then
        help "Losing configuration file: ${guomi_dir}/cert.cnf"
        exit 1
    fi
    #execute_cmd "cd GM && chmod a+x *"
    execute_cmd "cd ${guomi_dir}"
    ##callback gmchain.sh
    execute_cmd "./gmchain.sh ${dst_dir} ${manual}"
    execute_cmd "cd ${current_dir}"
}


##generate guomi chain cert
if [ ${enable_guomi} -eq 1 ];then
    LOG_INFO "Genearte Chain Certificates with Guomi Algorithm.."
    generate_guomi_cert ${ca_dir} ${guomi_dir}
    exit 0
fi

##generate ECDSA chain cert
if [  -f "$ca_dir/ca.key" ]; then
    echo "Attention! Duplicate generation of \"$ca_dir\ca.key\" and \"$ca_dir\ca.crt\". Overwrite?" 
    yes_go_other_exit
elif [  -f "$ca_dir/ca.crt" ]; then
    echo "Attention! Duplicate generation of \"$ca_dir\ca.key\" and \"$ca_dir\ca.crt\". Overwrite?" 
    yes_go_other_exit
fi
mkdir -p $ca_dir
generate_cert $ca_dir
LOG_INFO "Generate success! ca.crt ca.key has been generated into \"$ca_dir\""

