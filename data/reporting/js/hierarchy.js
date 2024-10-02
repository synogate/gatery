
function loadSerializedNodes() {
    const target = document.getElementById("nodeTarget");
    const scripts = ["data/nodes.js", "data/groups.js"];
    temp = document.getElementsByTagName("template")[0];
    nodeTemp = temp.content.querySelectorAll("p")[0];
    nodeTempLink = temp.content.querySelector("a");
    nodeButton = temp.content.querySelector("button");

    scripts.forEach(function(src) {
        var script = document.createElement('script');
        script.src = src;
        script.onerror = function () {
            console.log("Failed to load " + src);
        };
        document.head.append(script);
    });

    hierarchyNodeData.forEach(nodeItem => {
        node = document.importNode(nodeTemp, true);
        nodeLink = document.importNode(nodeTempLink, true);
        nodebtn = document.importNode(nodeButton, true);
        
        let nodeData = processEntity(nodeItem, false);
        fillOffcanvas(nodeItem, false);
        
        nodebtn.setAttribute("data-bs-content", nodeData);

        let nodeId = getId(nodeItem);
        if (nodeId >= 0) {
            nodeLink.href = `#node${nodeId}`;
        }
        
        for (const key in nodeItem) {
            if (Object.hasOwn(nodeItem, key)) {
                if (key == "id" || key == "name" || key == "group") {
                    const strongElement = document.createElement("strong");
                    strongElement.textContent = `${key}: `;
                    nodeLink.append(strongElement, nodeItem[key] + ", ");
                }
            }
        }
        node.append(nodeLink);
        node.append(nodebtn);
        target.append(node);
    })
}

function loadSerializedGroups() {
    const target = document.getElementById("groupTarget");
    temp = document.getElementsByTagName("template")[1];
    groupTemp = temp.content.querySelectorAll("p")[0];
    groupTempLink = temp.content.querySelector("a");
    groupButton = temp.content.querySelector("button");
    
    hierarchyGroupData.forEach(groupItem => {
        group = document.importNode(groupTemp, true);
        groupLink = document.importNode(groupTempLink, true);
        groupbtn = document.importNode(groupButton, true);
        
        let groupData = processEntity(groupItem);
        fillOffcanvas(groupItem, true);
        
        groupbtn.setAttribute("data-bs-content", groupData);

        let groupId = getId(groupItem);
        if (groupId >= 0) {
            groupLink.href = `#group${groupId}`;
        }
        
        for (const key in groupItem) {
            if (Object.hasOwn(groupItem, key)) {
                if (key == "id" || key == "name") {
                    const strongElement = document.createElement("strong");
                    strongElement.textContent = `${key}: `;
                    groupLink.append(strongElement, groupItem[key] + ", ");
                }
                if (key == "children" && !(groupItem[key].length == 0)) {
                    const strongElement = document.createElement("strong");
                    strongElement.textContent = `${key}: `;
                    
                    const value = groupItem[key];
                    var first = true;
                    for (i = 0; i < value.length; i++) {
                        if (first) {
                            first = false;
                            strongElement.textContent += `${value[i]}`;
                        } else {
                            strongElement.textContent += `, ${value[i]}`;
                        }
                        groupLink.append(strongElement);
                    }
                }
            }
        }
        group.append(groupLink);
        group.append(groupbtn);
        target.append(group);
    })
}

function processEntity(entity, showStackTrace) {
    let listHTML = "</ul>";
    listHTML += getObject(entity, listHTML, showStackTrace);
    listHTML += "</ul>";
    return listHTML;
}

function getObject(object, html, showStackTrace) {
    for (const key in object) {
        if (Object.hasOwn(object, key)) {
            const value = object[key];
            if (Array.isArray(value)) {
                html += `<li><strong>${key}:</strong><ul>`;
                if (value.length == 0) {
                    html += `<li>Empty</li>`;
                } else {
                    for (const item of value) {
                        if (key == "stack_trace") {
                            if (showStackTrace) {
                                html += `<li>${item}</li>`;
                            }
                        } else if (typeof item === "object" && item !== null) {
                            let innerHtml = '';
                            html += getObject(item, innerHtml);
                        } else {
                            html += `<li>${item}</li>`;
                        }
                    }
                }
                html += `</ul></li>`;
            } else {
                html += `<li><strong>${key}:</strong>`;
                if (typeof value === "object" && value !== null) {
                    let innerHtml = `<ul>`;
                    html += getObject(value, innerHtml);
                    html += `</ul>`;
                } else {
                    html += ` ${value}</li>`;
                }
            }
        }
    }
    return html;
}

function fillOffcanvas(node, isGroup) {
    const target = document.getElementById("offcanvasTarget")
    offcanvas = document.getElementsByTagName("template")[2];
    offcanvasContent = offcanvas.content;

    let nodeId = getId(node);
    if (nodeId >= 0) {
        if (isGroup) {
            offcanvas.content.querySelector('div').id = `group${nodeId}`;
        } else {
            offcanvas.content.querySelector('div').id = `node${nodeId}`;
        }
    }

    offcanvas.content.querySelector("h5").textContent = "Whole lotta data";

    if (node === undefined) {
        offcanvas.content.getElementById('undefined').innerHTML = `<p>This node has been removed during post processing</p>`;
    } else {
        offcanvas.content.getElementById('undefined').innerHTML = "";
        offcanvas.content.getElementById('id').innerHTML = `Node ID: ${node.id}`;
        offcanvas.content.getElementById('name').innerHTML = getNodeProperty(node, 'name', 'Name');
        if (isGroup) {
            offcanvas.content.getElementById('instanceName').innerHTML = getNodeProperty(node, 'instanceName', 'Instance Name');
            offcanvas.content.getElementById('stacktrace').innerHTML = getStacktrace(node["stack_trace"]);
            offcanvas.content.getElementById('children').innerHTML = getNodeProperty(node, 'children', 'Children');
        } else {
            offcanvas.content.getElementById('group').innerHTML = `Group: ${node.group}`;
            offcanvas.content.getElementById('stacktrace').innerHTML = getStacktrace(node["stack_trace"]);
            offcanvas.content.getElementById('type').innerHTML = `Type: ${node.type}`;
            offcanvas.content.getElementById('meta').innerHTML = getNodeProperty(node, 'meta', 'Meta information');
            offcanvas.content.getElementById('clocks').innerHTML = getNodeProperty(node, 'clocks', 'Clocks');
            offcanvas.content.getElementById('inputPorts').innerHTML = getNodeProperty(node, 'inputPorts', 'Input ports');
            offcanvas.content.getElementById('outputPorts').innerHTML = getNodeProperty(node, 'outputPorts', 'Output ports');
        }
    }

    nodeOffcanvas = document.importNode(offcanvasContent, true);
    target.append(nodeOffcanvas);

}

function getNodeProperty(node, propertyName, description) {
    if (!(propertyName in node) || node[propertyName] === "" || node[propertyName].length === 0) {
        return `${description}: No ${description} found for this node.`;
    } else if (Array.isArray(node[propertyName]) || typeof node[propertyName] === 'object') {
        return `${description}:\n<ul>${getObject(node[propertyName], "", false)}</ul>`;
    }
    return `${description}: ${node[propertyName]}`;
}

function getId(node) {
    for (const key in node) {
        if (key == "id") {
            return node[key];
        }
    }
    return -1;
}

function getStacktrace(stacktrace) {
    let stacktraceHTML = `<div class="">`;
    if (stacktrace.length === 0) {
        stacktraceHTML += `<button class="custom-btn btn-sm dropdown-toggle" type="button" data-bs-toggle="dropdown" aria-expanded="false">Stack Trace</button>`;
        stacktraceHTML += `<ul class="dropdown-menu dropdown-menu-end" style="background-color: #F8F9FA;">`;
        stacktraceHTML += `<p>No stacktrace found for this node.</p>`;
    } else {
        stacktraceHTML += `<button class="custom-btn btn-sm dropdown-toggle" type="button" data-bs-toggle="dropdown" aria-expanded="false">Stack Trace</button>`;
        stacktraceHTML += `<ul class="dropdown-menu dropdown-menu-end" style="background-color: #FFFFFF;">`;
        stacktrace.forEach(frame => {
            stacktraceHTML += `<li><a class="dropdown-item" href="vscode://file/Users/lucalichterman/Documents/Synogate/test_gatery_macos/gatery/source/gatery/debug/ReportInterface.h:45">${frame}</a></li>`;
        })
    }
    stacktraceHTML += `</ul></div>`;
    return stacktraceHTML;
}

function eventHander() {
    loadScripts().then(function() {
        loadSerializedNodes();
        loadSerializedGroups();
        initializePopovers();
    }).catch(function(error) {
        console.log(error);
    });
} 

function loadScripts() {
    const scripts = ["data/nodes.js", "data/groups.js"];
    const promises = scripts.map(function(src) {
        return new Promise(function(resolve, reject) {
            var script = document.createElement('script');
            script.src = src;
            script.onload = resolve;
            script.onerror = function() {
                fileNotAvailable(src);   
                console.log("Failed to load " + src);
                reject(Error("Failed to load " + src));
            };
            document.head.append(script);
        });
    });
    return Promise.all(promises);
}

function fileNotAvailable(file) {
    const target = document.getElementById("errorMessageTarget");
    const error = document.createElement("div");
    error.classList.add("alert");
    error.classList.add("alert-danger");
    error.setAttribute("role", "alert");
    error.textContent = "File " + file + " does not exist or cannot be found.";
    target.append(error);
}

function initializePopovers() {
    $('[data-bs-toggle="popover"]').popover({
        placement: 'right',
        trigger: 'click',
        html: true
    });
}

document.addEventListener("DOMContentLoaded", eventHander)