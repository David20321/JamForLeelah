using UnityEngine;
using System.Collections;

public class cameraFollow : MonoBehaviour {

	public Transform myTarget;
	public Vector3 offset;
	public float altitude;
	public float cameraRange = 1;
	public float camSpeed = 1;
	 
	void Start () {
		offset = transform.position - myTarget.transform.position;
		altitude = transform.position.y;
	}

	void Update () {

		transform.position = Vector3.Slerp(transform.position, myTarget.transform.position + offset, camSpeed * Time.deltaTime);

	}
}
